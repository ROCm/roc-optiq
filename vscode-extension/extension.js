// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
//
// Minimal VS Code extension that launches the existing roc-optiq desktop
// binary as a detached child process. No changes to the C++ project are
// required: this wrapper purely shells out to the executable and forwards
// the same CLI flags that `main.cpp` already accepts.

const vscode = require('vscode');
const cp = require('child_process');
const fs = require('fs');
const path = require('path');

const EXE_BASENAME = process.platform === 'win32' ? 'roc-optiq.exe' : 'roc-optiq';
const SUPPORTED_EXTS = new Set(['.rpd', '.db', '.yaml', '.rpv']);

const CANDIDATE_RELATIVE_PATHS = [
    path.join('build', 'x64-debug', 'Debug', EXE_BASENAME),
    path.join('build', 'x64-debug', 'Release', EXE_BASENAME),
    path.join('build', 'x64-release', 'Release', EXE_BASENAME),
    path.join('build', 'Debug', EXE_BASENAME),
    path.join('build', 'Release', EXE_BASENAME),
    path.join('build', EXE_BASENAME),
    path.join('out', 'build', 'x64-Debug', EXE_BASENAME),
    path.join('out', 'build', 'x64-Release', EXE_BASENAME),
];

function getConfig() {
    return vscode.workspace.getConfiguration('rocOptiq');
}

function expandHome(p) {
    if (!p) return p;
    if (p === '~') return process.env.HOME || process.env.USERPROFILE || p;
    if (p.startsWith('~/') || p.startsWith('~\\')) {
        const home = process.env.HOME || process.env.USERPROFILE;
        if (home) return path.join(home, p.slice(2));
    }
    return p;
}

function fileExists(p) {
    try {
        return p && fs.statSync(p).isFile();
    } catch (_e) {
        return false;
    }
}

function findExecutableInWorkspace() {
    const folders = vscode.workspace.workspaceFolders || [];
    for (const folder of folders) {
        const root = folder.uri.fsPath;
        for (const rel of CANDIDATE_RELATIVE_PATHS) {
            const candidate = path.join(root, rel);
            if (fileExists(candidate)) {
                return candidate;
            }
        }
    }
    return undefined;
}

function resolveExecutable() {
    const configured = expandHome(getConfig().get('executablePath') || '');
    if (configured) {
        if (fileExists(configured)) return configured;
        return { error: `Configured rocOptiq.executablePath does not exist: ${configured}` };
    }
    const detected = findExecutableInWorkspace();
    if (detected) return detected;
    return {
        error:
            `Could not locate ${EXE_BASENAME}. Set "rocOptiq.executablePath" in settings, ` +
            `or build the project so one of the standard build output folders contains the binary.`,
    };
}

function buildArgs(filePath) {
    const cfg = getConfig();
    const args = [];

    const backend = cfg.get('backend') || 'auto';
    if (backend && backend !== 'auto') {
        args.push('--backend', backend);
    }

    const fileDialog = cfg.get('fileDialog') || 'auto';
    if (fileDialog && fileDialog !== 'auto') {
        args.push('--file-dialog', fileDialog);
    }

    if (filePath) {
        args.push('--file', filePath);
    }

    const extra = cfg.get('extraArgs');
    if (Array.isArray(extra)) {
        for (const a of extra) {
            if (typeof a === 'string' && a.length > 0) args.push(a);
        }
    }

    return args;
}

function spawnVisualizer(exePath, args, output) {
    const cwd = path.dirname(exePath);
    output.appendLine(`> "${exePath}" ${args.map((a) => (a.includes(' ') ? `"${a}"` : a)).join(' ')}`);

    let child;
    try {
        child = cp.spawn(exePath, args, {
            cwd,
            detached: true,
            stdio: 'ignore',
            windowsHide: false,
        });
    } catch (err) {
        const msg = err && err.message ? err.message : String(err);
        output.appendLine(`Failed to spawn: ${msg}`);
        vscode.window.showErrorMessage(`ROCm Optiq: failed to launch (${msg})`);
        return;
    }

    child.on('error', (err) => {
        output.appendLine(`Process error: ${err.message}`);
        vscode.window.showErrorMessage(`ROCm Optiq: process error (${err.message})`);
    });

    if (typeof child.unref === 'function') {
        child.unref();
    }
}

async function pickTraceFile() {
    const uris = await vscode.window.showOpenDialog({
        canSelectFiles: true,
        canSelectFolders: false,
        canSelectMany: false,
        title: 'Open trace in ROCm Optiq',
        filters: {
            'ROCm Optiq files': ['rpd', 'db', 'yaml', 'rpv'],
            'All files': ['*'],
        },
    });
    if (!uris || uris.length === 0) return undefined;
    return uris[0].fsPath;
}

function isSupportedExt(p) {
    if (!p) return false;
    return SUPPORTED_EXTS.has(path.extname(p).toLowerCase());
}

async function launch(filePath, output) {
    const resolved = resolveExecutable();
    if (typeof resolved !== 'string') {
        const choice = await vscode.window.showErrorMessage(
            resolved.error,
            'Locate Executable...',
            'Open Settings'
        );
        if (choice === 'Locate Executable...') {
            await vscode.commands.executeCommand('rocOptiq.locateExecutable');
        } else if (choice === 'Open Settings') {
            await vscode.commands.executeCommand('workbench.action.openSettings', 'rocOptiq.executablePath');
        }
        return;
    }

    const args = buildArgs(filePath);
    spawnVisualizer(resolved, args, output);
}

// ---------------------------------------------------------------------------
// WebAssembly preview panel
// ---------------------------------------------------------------------------
//
// The Emscripten build uses pthreads (-pthread), which require the page to be
// cross-origin isolated (SharedArrayBuffer). A VS Code/Cursor webview cannot
// set the COEP/COOP HTTP headers on its own resources, so we run a tiny local
// HTTP server (src/app/wasm/serve.py) that serves the build dir with the
// right headers, and load it inside the webview via an <iframe> with
// `allow="cross-origin-isolated"`. The iframe is then properly isolated and
// pthreads work.
//
// The server is started lazily on first use and shut down on extension
// deactivate.

const WASM_CANDIDATE_DIRS = ['build-wasm'];
const DEFAULT_WASM_PORT = 8765;

let wasmServerProc = null;
let wasmServerPort = null;
let wasmServerCwd = null;
let activeWasmPanel = null;

function findWasmBuildDir() {
    const configured = expandHome(getConfig().get('wasmBuildDir') || '');
    if (configured) {
        if (fileExists(path.join(configured, 'index.html')) &&
            fileExists(path.join(configured, 'index.js')) &&
            fileExists(path.join(configured, 'index.wasm'))) {
            return configured;
        }
        return { error: `Configured rocOptiq.wasmBuildDir does not contain index.{html,js,wasm}: ${configured}` };
    }
    const folders = vscode.workspace.workspaceFolders || [];
    for (const folder of folders) {
        for (const rel of WASM_CANDIDATE_DIRS) {
            const candidate = path.join(folder.uri.fsPath, rel);
            if (fileExists(path.join(candidate, 'index.html')) &&
                fileExists(path.join(candidate, 'index.js')) &&
                fileExists(path.join(candidate, 'index.wasm'))) {
                return candidate;
            }
        }
    }
    return {
        error:
            'Could not locate a WebAssembly build. Build it first with ' +
            '`src/app/wasm/build.ps1`, or set "rocOptiq.wasmBuildDir" in settings.',
    };
}

function findRepoRootForBuildDir(buildDir) {
    let dir = path.resolve(buildDir, '..');
    for (let i = 0; i < 10; i++) {
        if (fileExists(path.join(dir, 'src', 'app', 'wasm', 'serve.py'))) return dir;
        const parent = path.dirname(dir);
        if (parent === dir) break;
        dir = parent;
    }
    const folders = vscode.workspace.workspaceFolders || [];
    for (const folder of folders) {
        const candidate = folder.uri.fsPath;
        if (fileExists(path.join(candidate, 'src', 'app', 'wasm', 'serve.py'))) return candidate;
    }
    return undefined;
}

function ensureWasmServer(buildDir, output) {
    if (wasmServerProc && wasmServerPort && wasmServerCwd === buildDir) {
        return wasmServerPort;
    }
    if (wasmServerProc) {
        try { wasmServerProc.kill(); } catch (_e) { /* ignore */ }
        wasmServerProc = null;
        wasmServerPort = null;
        wasmServerCwd = null;
    }

    const repoRoot = findRepoRootForBuildDir(buildDir);
    if (!repoRoot) {
        return { error: 'Could not locate src/app/wasm/serve.py to serve the WebAssembly build with COEP/COOP headers.' };
    }
    const servePy = path.join(repoRoot, 'src', 'app', 'wasm', 'serve.py');
    const port = DEFAULT_WASM_PORT;

    const python = process.platform === 'win32' ? 'python' : 'python3';
    output.appendLine(`> ${python} ${servePy} ${port} --dir ${buildDir}`);
    let proc;
    try {
        proc = cp.spawn(python, [servePy, String(port), '--dir', buildDir], {
            cwd: repoRoot,
            stdio: ['ignore', 'pipe', 'pipe'],
            windowsHide: true,
        });
    } catch (err) {
        return { error: `Failed to start local server: ${err && err.message ? err.message : err}` };
    }

    proc.stdout.on('data', (chunk) => output.append(`[serve] ${chunk.toString()}`));
    proc.stderr.on('data', (chunk) => output.append(`[serve] ${chunk.toString()}`));
    proc.on('exit', (code) => {
        output.appendLine(`[serve] exited (code=${code})`);
        if (wasmServerProc === proc) {
            wasmServerProc = null;
            wasmServerPort = null;
            wasmServerCwd = null;
        }
    });

    wasmServerProc = proc;
    wasmServerPort = port;
    wasmServerCwd = buildDir;
    return port;
}

function buildHostHtml(port, cspSource) {
    const iframeUrl = `http://127.0.0.1:${port}/index.html`;
    const csp = [
        "default-src 'none'",
        `frame-src http://127.0.0.1:* http://localhost:*`,
        `style-src ${cspSource} 'unsafe-inline'`,
        `script-src ${cspSource} 'unsafe-inline'`,
    ].join('; ');

    return `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta http-equiv="Content-Security-Policy" content="${csp}">
    <title>ROCm Optiq</title>
    <style>
        html, body { margin: 0; padding: 0; height: 100%; background: #1e1f22; color: #e6e6e6;
                     font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
        iframe { display: block; width: 100vw; height: calc(100vh - 36px); border: 0; background: #1e1f22; }
        #toolbar { height: 36px; display: flex; align-items: center; gap: 8px; padding: 0 10px;
                   box-sizing: border-box; border-bottom: 1px solid rgba(255,255,255,0.12); background: #252526; }
        button { color: inherit; background: #3c3c3c; border: 1px solid #5a5a5a; border-radius: 3px;
                 padding: 4px 10px; cursor: pointer; }
        button:hover { background: #454545; }
        #hint { opacity: 0.75; font-size: 12px; }
        #fallback { position: absolute; inset: 0; display: none; align-items: center; justify-content: center;
                    text-align: center; padding: 24px; box-sizing: border-box; }
        #fallback.show { display: flex; }
    </style>
</head>
<body>
    <div id="toolbar">
        <button id="openFile">Open File...</button>
        <span id="hint">Use this button for disk files. The in-app ImGui dialog can only see the WASM virtual filesystem.</span>
    </div>
    <iframe id="wasm" src="${iframeUrl}"
            allow="cross-origin-isolated"
            sandbox="allow-scripts allow-same-origin"
            onerror="document.getElementById('fallback').classList.add('show')"></iframe>
    <div id="fallback">
        <div>
            <h2>ROCm Optiq could not load</h2>
            <p>Local WASM server isn't reachable at <code>${iframeUrl}</code>.</p>
            <p>Run <code>python src/app/wasm/serve.py</code> from the repo root, then run<br>
               <strong>ROCm Optiq: Open in Panel</strong> again.</p>
        </div>
    </div>
    <script>
        const vscode = acquireVsCodeApi();
        const iframe = document.getElementById('wasm');
        const targetOrigin = 'http://127.0.0.1:${port}';
        let iframeReady = false;
        const pending = [];

        function sendToWasm(message) {
            if (!iframeReady) {
                pending.push(message);
                return;
            }
            iframe.contentWindow.postMessage(message, targetOrigin);
        }

        iframe.addEventListener('load', () => {
            iframeReady = true;
            while (pending.length > 0) {
                sendToWasm(pending.shift());
            }
        });

        document.getElementById('openFile').addEventListener('click', () => {
            vscode.postMessage({ type: 'pickFile' });
        });

        window.addEventListener('message', (event) => {
            const data = event.data;
            if (!data || typeof data !== 'object') return;

            if (data.type === 'rocOptiq:openFile') {
                sendToWasm(data);
            } else if (data.type === 'rocOptiq:fileOpened' ||
                       data.type === 'rocOptiq:fileOpenError' ||
                       data.type === 'rocOptiq:log') {
                vscode.postMessage(data);
            }
        });
    </script>
</body>
</html>`;
}

function uriFromCommandArg(uriArg) {
    if (uriArg && typeof uriArg === 'object' && uriArg.fsPath) {
        return uriArg;
    }
    return undefined;
}

async function pickTraceUri() {
    const filePath = await pickTraceFile();
    return filePath ? vscode.Uri.file(filePath) : undefined;
}

async function postFileToWasmPanel(panel, uriArg, output) {
    let uri = uriFromCommandArg(uriArg);
    if (!uri) {
        uri = await pickTraceUri();
    }
    if (!uri) return;

    const filePath = uri.fsPath;
    if (!isSupportedExt(filePath)) {
        const proceed = await vscode.window.showWarningMessage(
            `ROCm Optiq: ${path.basename(filePath)} is not a recognised trace type ` +
                `(.rpd, .db, .yaml, .rpv). Open in panel anyway?`,
            'Open Anyway',
            'Cancel'
        );
        if (proceed !== 'Open Anyway') return;
    }

    const bytes = await vscode.workspace.fs.readFile(uri);
    const data = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
    output.appendLine(`Posting ${filePath} (${bytes.byteLength} bytes) to WASM panel`);
    await panel.webview.postMessage({
        type: 'rocOptiq:openFile',
        name: path.basename(filePath),
        data,
    });
}

function openWasmPanel(context, output) {
    const resolved = findWasmBuildDir();
    if (typeof resolved !== 'string') {
        vscode.window.showErrorMessage(resolved.error, 'Open Settings').then((choice) => {
            if (choice === 'Open Settings') {
                vscode.commands.executeCommand('workbench.action.openSettings', 'rocOptiq.wasmBuildDir');
            }
        });
        return;
    }

    const portOrErr = ensureWasmServer(resolved, output);
    if (typeof portOrErr !== 'number') {
        vscode.window.showErrorMessage(portOrErr.error);
        return;
    }

    if (activeWasmPanel) {
        activeWasmPanel.reveal(vscode.ViewColumn.Active);
        return activeWasmPanel;
    }

    const panel = vscode.window.createWebviewPanel(
        'rocOptiqWasm',
        'ROCm Optiq',
        vscode.ViewColumn.Active,
        {
            enableScripts: true,
            retainContextWhenHidden: true,
        }
    );

    activeWasmPanel = panel;
    panel.onDidDispose(() => {
        if (activeWasmPanel === panel) activeWasmPanel = null;
    }, undefined, context.subscriptions);
    panel.webview.onDidReceiveMessage(async (message) => {
        if (!message || typeof message !== 'object') return;

        if (message.type === 'pickFile') {
            await postFileToWasmPanel(panel, undefined, output);
        } else if (message.type === 'rocOptiq:log') {
            const level = message.level || 'log';
            const text = message.message || '';
            output.appendLine(`[wasm:${level}] ${text}`);
        } else if (message.type === 'rocOptiq:fileOpened') {
            output.appendLine(`WASM opened ${message.name} as ${message.virtualPath}`);
            vscode.window.showInformationMessage(`ROCm Optiq: opened ${message.name} in panel.`);
        } else if (message.type === 'rocOptiq:fileOpenError') {
            const msg = message.message || 'unknown error';
            output.appendLine(`WASM open failed: ${msg}`);
            output.show(true);
            vscode.window.showErrorMessage(`ROCm Optiq: failed to open file in panel (${msg})`);
        }
    }, undefined, context.subscriptions);

    panel.webview.html = buildHostHtml(portOrErr, panel.webview.cspSource);
    context.subscriptions.push(panel);
    return panel;
}

async function openFileInWasmPanel(context, output, uriArg) {
    const panel = openWasmPanel(context, output);
    if (!panel) return;
    await postFileToWasmPanel(panel, uriArg, output);
}

function shutdownWasmServer() {
    activeWasmPanel = null;
    if (wasmServerProc) {
        try { wasmServerProc.kill(); } catch (_e) { /* ignore */ }
        wasmServerProc = null;
        wasmServerPort = null;
        wasmServerCwd = null;
    }
}

function activate(context) {
    const output = vscode.window.createOutputChannel('ROCm Optiq');
    context.subscriptions.push(output);

    context.subscriptions.push(
        vscode.commands.registerCommand('rocOptiq.openInPanel', () => openWasmPanel(context, output))
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('rocOptiq.openFileInPanel',
            async (uriArg) => openFileInWasmPanel(context, output, uriArg))
    );

    context.subscriptions.push({ dispose: shutdownWasmServer });

    context.subscriptions.push(
        vscode.commands.registerCommand('rocOptiq.openVisualizer', async () => {
            await launch(undefined, output);
        })
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('rocOptiq.openFile', async (uriArg) => {
            let filePath;
            if (uriArg && typeof uriArg === 'object' && uriArg.fsPath) {
                filePath = uriArg.fsPath;
            } else {
                filePath = await pickTraceFile();
            }
            if (!filePath) return;
            await launch(filePath, output);
        })
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('rocOptiq.openActiveFile', async () => {
            const editor = vscode.window.activeTextEditor;
            const doc = editor && editor.document;
            const filePath = doc && doc.uri && doc.uri.scheme === 'file' ? doc.uri.fsPath : undefined;
            if (!filePath) {
                vscode.window.showWarningMessage('ROCm Optiq: no active file to open.');
                return;
            }
            if (!isSupportedExt(filePath)) {
                const proceed = await vscode.window.showWarningMessage(
                    `ROCm Optiq: ${path.basename(filePath)} is not a recognised trace type ` +
                        `(.rpd, .db, .yaml, .rpv). Open anyway?`,
                    'Open Anyway',
                    'Cancel'
                );
                if (proceed !== 'Open Anyway') return;
            }
            await launch(filePath, output);
        })
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('rocOptiq.locateExecutable', async () => {
            const uris = await vscode.window.showOpenDialog({
                canSelectFiles: true,
                canSelectFolders: false,
                canSelectMany: false,
                title: `Select the ${EXE_BASENAME} binary`,
                filters:
                    process.platform === 'win32'
                        ? { Executables: ['exe'], 'All files': ['*'] }
                        : { 'All files': ['*'] },
            });
            if (!uris || uris.length === 0) return;
            const chosen = uris[0].fsPath;
            await getConfig().update(
                'executablePath',
                chosen,
                vscode.ConfigurationTarget.Global
            );
            vscode.window.showInformationMessage(`ROCm Optiq: executable set to ${chosen}`);
        })
    );
}

function deactivate() {
    shutdownWasmServer();
}

module.exports = { activate, deactivate };
