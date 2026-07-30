import type { Track, TrackEvent, TrackSample } from '../api/types';
import { entrySpan, isSampleEntry } from '../api/types';
import {
  formatDuration,
  formatTimestamp,
  idToString,
  trackLabel,
  trackSubLabel,
} from '../lib/format';

export interface ExtDataItem {
  category?: string;
  name?: string;
  value?: string;
}

interface Props {
  track: Track;
  entry: TrackEvent | TrackSample;
  origin: number;
  extData: ExtDataItem[] | null;
  extBusy: boolean;
  onLoadExt: () => void;
  onClose: () => void;
}

export function DetailsPanel({
  track,
  entry,
  origin,
  extData,
  extBusy,
  onLoadExt,
  onClose,
}: Props) {
  const [start, end] = entrySpan(entry);
  const sample = isSampleEntry(entry);

  return (
    <div className="details">
      <div className="row" style={{ marginBottom: 9 }}>
        <strong style={{ fontSize: 13 }}>
          {sample ? trackLabel(track) : (entry.name ?? '(unnamed)')}
        </strong>
        <span className="spacer" />
        {!sample && (
          <button onClick={onLoadExt} disabled={extBusy}>
            {extBusy ? 'Loading…' : 'Extended data'}
          </button>
        )}
        <button onClick={onClose}>Close</button>
      </div>

      <dl className="kv">
        <dt>Track</dt>
        <dd>
          {trackLabel(track)}
          {trackSubLabel(track) ? ` · ${trackSubLabel(track)}` : ''}
        </dd>
        <dt>Id</dt>
        <dd>{idToString(entry.id)}</dd>
        <dt>Start</dt>
        <dd>{formatTimestamp(start, origin)}</dd>
        {!sample && (
          <>
            <dt>Duration</dt>
            <dd>{formatDuration(end - start)}</dd>
          </>
        )}
        {sample && (
          <>
            <dt>Value</dt>
            <dd>{(entry as TrackSample).value}</dd>
          </>
        )}
        {!sample && (entry as TrackEvent).category && (
          <>
            <dt>Category</dt>
            <dd>{(entry as TrackEvent).category}</dd>
          </>
        )}
        {!sample && (entry as TrackEvent).level !== undefined && (
          <>
            <dt>Level</dt>
            <dd>{(entry as TrackEvent).level}</dd>
          </>
        )}
      </dl>

      {extData && extData.length > 0 && (
        <>
          <div className="pre-label">Extended data</div>
          <dl className="kv">
            {extData.map((item, index) => (
              <FragmentRow key={index} item={item} />
            ))}
          </dl>
        </>
      )}
      {extData && extData.length === 0 && (
        <div className="hint" style={{ marginTop: 8 }}>
          This event has no extended properties.
        </div>
      )}
    </div>
  );
}

function FragmentRow({ item }: { item: ExtDataItem }) {
  return (
    <>
      <dt>{item.name ?? item.category ?? 'value'}</dt>
      <dd>{item.value ?? '—'}</dd>
    </>
  );
}
