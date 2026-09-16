// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The editor half of the Optiq closed loop. Optiq analyses the trace and
// decides what to change; this applies the change the user approved and runs
// the build behind it, so there is one writer for the source tree rather than
// Optiq reaching into the workspace behind the editor's back.
//
// It listens on loopback with an ephemeral port and drops a handshake file in
// Optiq's own config directory. Optiq is the client; nothing here dials out.

import { exec } from 'child_process';
import { randomUUID } from 'crypto';
import * as fs from 'fs';
import * as http from 'http';
import * as os from 'os';
import * as path from 'path';
import * as vscode from 'vscode';

const HANDSHAKE_FILE = 'optiq-ide.json';
const TOKEN_HEADER = 'x-optiq-token';
// Enough compiler output to find the error; Optiq trims it again before it
// reaches the model.
const MAX_BUILD_OUTPUT = 60000;

// Mirrors get_application_config_path() in src/view/src/rocprofvis_utils.cpp.
// The two have to agree on this path or Optiq never finds the handshake.
function optiqConfigDir(): string {
    if (process.platform === 'win32') {
        return path.join(process.env.LOCALAPPDATA ?? os.homedir(), 'AMD', 'ROCm-Optiq');
    }
    if (process.platform === 'darwin') {
        return path.join(os.homedir(), 'Library', 'Application Support', 'ROCm-Optiq');
    }
    const base = process.env.XDG_CONFIG_HOME ?? path.join(os.homedir(), '.config');
    return path.join(base, 'rocm-optiq');
}

function handshakePath(): string {
    return path.join(optiqConfigDir(), HANDSHAKE_FILE);
}

// Asks VS Code to forward our port to the machine the user is sitting at. In a
// local window this hands back the same address. In a Remote-SSH window we are
// running on the remote, and this is the whole reason Optiq can reach us at
// all: it returns a localhost URL on the client side, tunnelled by VS Code, so
// no SSH port forwarding has to be built anywhere.
async function tunnelUrl(port: number): Promise<string> {
    try {
        const external = await vscode.env.asExternalUri(
            vscode.Uri.parse(`http://127.0.0.1:${port}`)
        );
        return external.toString();
    } catch {
        return '';
    }
}

async function writeHandshake(port: number, token: string): Promise<void> {
    const folder = vscode.workspace.workspaceFolders?.[0];
    // Set when the workspace lives on another machine over Remote-SSH. Optiq
    // reads it to know this handshake describes a tunnel rather than a port on
    // its own host.
    const remote = vscode.env.remoteName ?? '';
    const handshake = {
        port,
        token,
        workspace: folder ? folder.uri.fsPath : '',
        remote,
        externalUrl: await tunnelUrl(port)
    };
    fs.mkdirSync(optiqConfigDir(), { recursive: true });
    fs.writeFileSync(handshakePath(), JSON.stringify(handshake, null, 2));
    if (remote && !handshake.externalUrl) {
        vscode.window.showWarningMessage(
            'Optiq loop: this is a remote window and the port could not be forwarded, ' +
                'so Optiq will not be able to reach this editor.'
        );
    }
}

// A missing handshake is how Optiq knows no editor is attached, so it goes on
// shutdown rather than being left behind pointing at a dead port.
function removeHandshake(): void {
    try {
        fs.unlinkSync(handshakePath());
    } catch {
        // Already gone, which is the state we wanted.
    }
}

function resolveInWorkspace(file: string): vscode.Uri | undefined {
    const folders = vscode.workspace.workspaceFolders;
    if (!folders?.length) {
        return undefined;
    }
    if (path.isAbsolute(file)) {
        return vscode.Uri.file(file);
    }
    // First root that actually holds it, so a multi-root workspace resolves the
    // way the user would expect instead of always taking the first.
    for (const folder of folders) {
        const candidate = vscode.Uri.joinPath(folder.uri, file);
        if (fs.existsSync(candidate.fsPath)) {
            return candidate;
        }
    }
    return vscode.Uri.joinPath(folders[0].uri, file);
}

// Enough candidates to recognise the right one, few enough that a vague query
// does not come back as a directory listing.
const MAX_FIND_RESULTS = 40;
// How many files a content search will open. The extension host runs beside the
// source, so these are local reads, but a monorepo still deserves a ceiling.
const MAX_SCAN_FILES = 600;
const MAX_CONTENT_HITS = 40;
const EXCLUDE_GLOB = '**/{node_modules,.git,build,out,dist,target,.venv,__pycache__}/**';
const SOURCE_GLOB = '**/*.{c,cc,cpp,cxx,h,hh,hpp,hip,cu,py,rs,go,java,ts,js,cmake,txt,sh}';

/**
 * Finds where something lives, by file name and by content.
 *
 * Content search is the half that matters. What a trace hands the assistant is
 * a kernel or function name - `op_scale` - and there is no file called that;
 * it is a symbol inside one. Matching names alone answers "no such file" to a
 * question that had a perfectly good answer, which reads as the source being
 * missing rather than the search being wrong.
 */
async function findSource(query: string): Promise<object> {
    if (!query) {
        return { ok: false, error: 'find_source needs something to search for.' };
    }

    const namePattern = query.includes('*') ? query : `**/*${query}*`;
    const named = await vscode.workspace.findFiles(
        namePattern,
        EXCLUDE_GLOB,
        MAX_FIND_RESULTS
    );

    const matches: { file: string; line: number; text: string }[] = [];
    const candidates = await vscode.workspace.findFiles(
        SOURCE_GLOB,
        EXCLUDE_GLOB,
        MAX_SCAN_FILES
    );
    for (const uri of candidates) {
        if (matches.length >= MAX_CONTENT_HITS) {
            break;
        }
        let text: string;
        try {
            text = Buffer.from(await vscode.workspace.fs.readFile(uri)).toString('utf8');
        } catch {
            continue;
        }
        if (!text.includes(query)) {
            continue;
        }
        const relative = vscode.workspace.asRelativePath(uri, false);
        const lines = text.split('\n');
        for (let i = 0; i < lines.length && matches.length < MAX_CONTENT_HITS; i++) {
            if (lines[i].includes(query)) {
                matches.push({ file: relative, line: i + 1, text: lines[i].trim() });
            }
        }
    }

    return {
        ok: true,
        files: named.map((uri) => vscode.workspace.asRelativePath(uri, false)),
        matches
    };
}

async function readSource(file: string): Promise<object> {
    const uri = resolveInWorkspace(file);
    if (!uri) {
        return { ok: false, error: 'No folder is open in the editor.' };
    }
    const document = await vscode.workspace.openTextDocument(uri);
    return { ok: true, text: document.getText() };
}

/**
 * Runs the workspace's build command after an edit.
 *
 * The file that was just changed is exported to it, because a project with many
 * independent targets cannot name one in a static command: a command hardwired
 * to one target rebuilds that target, reports success, and leaves the binary
 * that is about to be profiled untouched - which reads as "the fix did nothing"
 * rather than "the fix was never compiled".
 */
function runBuild(changedFile: string): Promise<{ ok: boolean; output: string }> {
    const command = vscode.workspace
        .getConfiguration('optiq')
        .get<string>('buildCommand', '')
        .trim();
    const folder = vscode.workspace.workspaceFolders?.[0];
    if (!command || !folder) {
        return Promise.resolve({
            ok: false,
            output:
                'No build command is set, so the change was not compiled. Set ' +
                'optiq.buildCommand in settings, or rebuild by hand before ' +
                'profiling again.'
        });
    }

    const relative = changedFile.replace(/\\/g, '/');
    const base = relative.slice(relative.lastIndexOf('/') + 1);
    const dot = base.lastIndexOf('.');
    const env = {
        ...process.env,
        OPTIQ_FILE: relative,
        OPTIQ_STEM: dot > 0 ? base.slice(0, dot) : base,
        OPTIQ_DIR: relative.includes('/') ? relative.slice(0, relative.lastIndexOf('/')) : '.'
    };

    return new Promise((resolve) => {
        exec(
            command,
            { cwd: folder.uri.fsPath, env, maxBuffer: MAX_BUILD_OUTPUT * 4 },
            (error, stdout, stderr) => {
                const header = `$ ${command}\n  (OPTIQ_FILE=${env.OPTIQ_FILE} OPTIQ_STEM=${env.OPTIQ_STEM})\n`;
                const output = `${header}${stdout}${stderr}`.slice(0, MAX_BUILD_OUTPUT);
                resolve({ ok: error === null, output });
            }
        );
    });
}

async function applyChange(request: {
    file?: string;
    find?: string;
    replace?: string;
    build?: boolean;
}): Promise<object> {
    const uri = resolveInWorkspace(request.file ?? '');
    if (!uri) {
        return { applied: false, error: 'No folder is open in the editor.' };
    }

    const document = await vscode.workspace.openTextDocument(uri);
    const text = document.getText();
    const find = request.find ?? '';
    const at = text.indexOf(find);
    if (at < 0) {
        return { applied: false, error: `That text is not in ${request.file}.` };
    }
    // A change that could land in two places is one the user cannot check at a
    // glance, so it is refused rather than guessed at.
    if (text.indexOf(find, at + 1) >= 0) {
        return {
            applied: false,
            error: `That text appears more than once in ${request.file}. Include more surrounding lines so it is unique.`
        };
    }

    const edit = new vscode.WorkspaceEdit();
    const range = new vscode.Range(document.positionAt(at), document.positionAt(at + find.length));
    edit.replace(uri, range, request.replace ?? '');
    if (!(await vscode.workspace.applyEdit(edit))) {
        return { applied: false, error: 'The editor refused the edit.' };
    }
    await document.save();
    // Put the user on the change, so an approved edit is one they can see
    // rather than one that happened somewhere off screen.
    await vscode.window.showTextDocument(document, { preview: false });

    if (request.build !== true) {
        return { applied: true, built: true, output: 'No build was asked for.' };
    }
    const build = await runBuild(vscode.workspace.asRelativePath(uri, false));
    return { applied: true, built: build.ok, output: build.output };
}

export function activate(context: vscode.ExtensionContext): void {
    const token = randomUUID();

    const server = http.createServer((req, res) => {
        let body = '';
        req.on('data', (chunk) => {
            body += chunk;
        });
        req.on('end', async () => {
            const reply = (status: number, payload: object): void => {
                res.writeHead(status, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify(payload));
            };
            if (req.headers[TOKEN_HEADER] !== token) {
                reply(403, { ok: false, error: 'Bad token.' });
                return;
            }
            try {
                const request = body ? JSON.parse(body) : {};
                if (req.url === '/read') {
                    reply(200, await readSource(request.file ?? ''));
                } else if (req.url === '/find') {
                    reply(200, await findSource(request.query ?? ''));
                } else if (req.url === '/apply') {
                    reply(200, await applyChange(request));
                } else {
                    reply(404, { ok: false, error: 'Unknown request.' });
                }
            } catch (err) {
                // Answered as a result rather than a dropped socket, so Optiq
                // can tell the user what went wrong instead of timing out.
                reply(200, { ok: false, applied: false, error: String(err) });
            }
        });
    });

    // Loopback and an ephemeral port: nothing off this machine can reach it,
    // and the handshake is how Optiq learns which port it got.
    server.listen(0, '127.0.0.1', () => {
        const address = server.address();
        const port = typeof address === 'object' && address !== null ? address.port : 0;
        void writeHandshake(port, token).then(() => {
            vscode.window.setStatusBarMessage(`Optiq loop attached (port ${port})`, 5000);
        });
    });

    context.subscriptions.push({
        dispose: () => {
            server.close();
            removeHandshake();
        }
    });
}

export function deactivate(): void {
    removeHandshake();
}
