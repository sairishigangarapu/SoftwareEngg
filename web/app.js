/*
 * UATC web frontend (FR-13, FR-14, FR-15; SAD 4.5; docs/wireframes.md).
 *
 * Presentation only: no compression logic lives here. Every operation is
 * a call to the C++ backend's HTTP API (docs/api-contract.md).
 *
 * Modes
 *   live  (?live)  talks to `uatc serve`. Open the URL the server prints:
 *                  http://127.0.0.1:8080/?live#token=...
 *   mock  (default) no backend at all; canned data so the screens can be
 *                  demonstrated and tested anywhere. Add ?fail to see the
 *                  error screen.
 *
 * Element IDs are stable for the Selenium tests (wireframes.md section 10).
 */
'use strict';

// ------------------------------------------------------------ config

const PARAMS = new URLSearchParams(location.search);
const USE_MOCK = !PARAMS.has('live');
const MOCK_FAIL = PARAMS.has('fail');

// When `uatc serve` serves this page, the API is on the same origin. From
// the separate dev server on port 8000 it is http://127.0.0.1:8080, which
// the server allows as an origin.
const API = USE_MOCK
  ? '/api'
  : (location.port === '8000' ? 'http://127.0.0.1:8080' : '') + '/api';

// The session token `uatc serve` prints (SAD 3.9). It travels in the URL
// fragment (#token=...), which the browser never sends to any server, and
// goes back only in the X-UATC-Token header.
const TOKEN = new URLSearchParams(location.hash.slice(1)).get('token') || '';

const POLL_MS = 500;   // ADR-07

// SAD 4.3.4, "Plain-language message (UI)" column, word for word. Used only
// when a reply carries no userMessage of its own. The UI never shows the
// raw code (TC-UI-04).
const MESSAGES = {
  E_INVALID_ARGUMENT: 'Something in the request is not valid. Please check the file or folder you selected.',
  E_NOT_FOUND: 'We could not find that file or folder.',
  E_PERMISSION: 'UATC is not allowed to open or write that location.',
  E_IO: 'The operation stopped because the disk could not be read or written (it may be full).',
  E_DISK_FULL: 'The operation stopped because the disk could not be read or written (it may be full).',
  E_MALFORMED_ARCHIVE: 'This file is not a valid UATC archive or is damaged.',
  E_UNSUPPORTED_VERSION: 'This archive was made by a newer version of UATC.',
  E_MISSING_BLOCK: 'Part of this archive is missing.',
  E_BLOCK_CHECKSUM: 'This archive is corrupted and cannot be extracted safely.',
  E_ARCHIVE_CHECKSUM: 'This archive failed its integrity check.',
  E_PATH_TRAVERSAL: 'The archive contains an unsafe file path and was stopped.',
  E_INTERNAL: 'Something went wrong inside UATC. Please try again.'
};

const CONNECTION_MESSAGE =
  'UATC could not reach its background service. Make sure `uatc serve` is ' +
  'running, then open the address it prints.';

// Per-operation wording for the Select and Progress screens.
const OPERATIONS = {
  compress: {
    title: 'Compress',
    sourceLabel: 'Source (file or folder)',
    sourceKind: 'file-or-folder',
    destinationLabel: 'Destination archive',
    destinationHelp: 'Where the new .uatc archive will be saved.',
    destinationKind: 'save-archive',
    start: 'Compress',
    working: 'Compressing…'
  },
  extract: {
    title: 'Extract',
    sourceLabel: 'Archive to extract',
    sourceKind: 'file',
    destinationLabel: 'Extract into folder',
    destinationHelp: 'The original files will be restored inside this folder.',
    destinationKind: 'folder',
    start: 'Extract',
    working: 'Extracting…'
  },
  info: {
    title: 'Archive Info',
    sourceLabel: 'Archive',
    sourceKind: 'file',
    start: 'Show info'
  },
  verify: {
    title: 'Verify Archive',
    sourceLabel: 'Archive to check',
    sourceKind: 'file',
    start: 'Check archive',
    working: 'Checking…'
  }
};

// ------------------------------------------------------------ mock data

const MOCK_ROOT = '/home/demo';
const MOCK_FS = {
  '/home/demo': [
    { name: 'photos', type: 'directory', size: 0 },
    { name: 'project', type: 'directory', size: 0 },
    { name: 'notes.txt', type: 'file', size: 18234 },
    { name: 'project.uatc', type: 'file', size: 412000000 }
  ],
  '/home/demo/project': [
    { name: 'src', type: 'directory', size: 0 },
    { name: 'README.md', type: 'file', size: 4210 },
    { name: 'data.csv', type: 'file', size: 734000000 }
  ],
  '/home/demo/project/src': [
    { name: 'main.cpp', type: 'file', size: 12045 }
  ],
  '/home/demo/photos': [
    { name: 'beach.jpg', type: 'file', size: 2400000 }
  ]
};

const MOCK_USAGE = [
  { id: 0, name: 'None', blocks: 300 },
  { id: 1, name: 'Delta', blocks: 120 },
  { id: 2, name: 'XOR', blocks: 30 },
  { id: 3, name: 'Byte-shuffle', blocks: 50 },
  { id: 4, name: 'RLE', blocks: 200 }
];

// ------------------------------------------------------------ state

const $ = id => document.getElementById(id);

let operation = 'compress';
let pollTimer = null;
let elapsedTimer = null;
let jobGeneration = 0;      // stale poll replies are ignored
let startedAt = 0;
let mockJob = null;

// ------------------------------------------------------------ helpers

function show(screenName) {
  document.querySelectorAll('.screen').forEach(screen => {
    screen.hidden = screen.id !== 'screen-' + screenName;
  });
  // Move focus to the heading so keyboard and screen-reader users land on
  // the new screen (wireframes.md section 10).
  $('screen-' + screenName)?.querySelector('h2')?.focus();
}

function formatBytes(bytes) {
  const value = Number(bytes);
  if (!Number.isFinite(value) || value < 0) return 'Unknown';
  const units = ['B', 'KB', 'MB', 'GB', 'TB'];
  let size = value;
  let index = 0;
  while (size >= 1024 && index < units.length - 1) {
    size /= 1024;
    index++;
  }
  return size.toFixed(index === 0 ? 0 : 1) + ' ' + units[index];
}

function formatClock(ms) {
  const total = Math.max(0, Math.floor(ms / 1000));
  const m = String(Math.floor(total / 60)).padStart(2, '0');
  const s = String(total % 60).padStart(2, '0');
  return m + ':' + s;
}

function formatSeconds(ms) {
  const value = Number(ms);
  if (!Number.isFinite(value)) return 'Unknown';
  return value < 1000 ? value + ' ms' : (value / 1000).toFixed(1) + ' s';
}

function formatRatio(ratio) {
  const value = Number(ratio);
  return Number.isFinite(value) && value > 0 ? value.toFixed(2) : 'N/A';
}

function escapeHtml(value) {
  return String(value ?? '').replace(/[&<>"']/g, c => ({
    '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
  })[c]);
}

// A two-column key/value table. Values are escaped.
function renderPairs(rows) {
  return `<table><tbody>${rows.map(([label, value]) => `
    <tr><th scope="row">${escapeHtml(label)}</th><td>${escapeHtml(value)}</td></tr>`
  ).join('')}</tbody></table>`;
}

// The "which method was used" table (SAD 4.5 Result screen).
function renderUsage(usage, caption) {
  const rows = (usage || []).filter(u => Number(u.blocks) > 0);
  if (rows.length === 0) return '';
  return `
    <h3>${escapeHtml(caption)}</h3>
    <table>
      <thead><tr><th scope="col">Method</th><th scope="col">Blocks</th></tr></thead>
      <tbody>${rows.map(u => `
        <tr><td>${escapeHtml(u.name)}</td><td class="num">${escapeHtml(u.blocks)}</td></tr>`
      ).join('')}</tbody>
    </table>`;
}

const STORED_EXPLANATION =
  'Some parts were stored as-is because compressing them would not have ' +
  'made them smaller. This is normal for photos, videos and files that are ' +
  'already compressed, and nothing is lost.';

function mockNote(text) {
  return USE_MOCK ? `<p class="banner"><strong>Demo only:</strong> ${escapeHtml(text)}</p>` : '';
}

// Path helpers. The backend reports absolute POSIX paths.

// Removes trailing '/' characters with a plain loop (a regex like /\/+$/
// can backtrack badly on long runs of slashes).
function stripTrailingSlashes(path) {
  let end = path.length;
  while (end > 1 && path[end - 1] === '/') end--;
  return path.slice(0, end);
}

function dirname(path) {
  const trimmed = stripTrailingSlashes(path);
  const at = trimmed.lastIndexOf('/');
  return at <= 0 ? '/' : trimmed.slice(0, at);
}
function basename(path) {
  const trimmed = stripTrailingSlashes(path);
  return trimmed.slice(trimmed.lastIndexOf('/') + 1);
}
function joinPath(dir, name) {
  return stripTrailingSlashes(dir) + '/' + name;
}

// The plain-language text for any failure object (TC-UI-04).
function userMessageFor(error) {
  return error?.userMessage || MESSAGES[error?.code] || MESSAGES.E_INTERNAL;
}

// ------------------------------------------------------------ API

async function api(method, path, body) {
  if (USE_MOCK) return mockApi(method, path, body);

  const options = {
    method,
    headers: { 'X-UATC-Token': TOKEN }
  };
  if (body !== undefined) {
    options.headers['Content-Type'] = 'application/json';
    options.body = JSON.stringify(body);
  }

  let response;
  try {
    response = await fetch(API + path, options);
  } catch {
    throw { code: 'E_INTERNAL', userMessage: CONNECTION_MESSAGE };
  }

  let data;
  try {
    data = await response.json();
  } catch {
    throw { code: 'E_INTERNAL' };
  }

  if (!response.ok) {
    throw data?.error || { code: 'E_INTERNAL' };
  }
  return data;
}

// A small stand-in for the backend with the same reply shapes.
async function mockApi(method, path, body) {
  await new Promise(resolve => setTimeout(resolve, 120));
  // Split "/fs/list?path=..." by hand: no URL object, no base address.
  const [pathname, queryString = ''] = path.split('?');
  const query = new URLSearchParams(queryString);

  if (method === 'GET' && pathname === '/fs/list') {
    const dir = query.get('path') || MOCK_ROOT;
    if (!dir.startsWith(MOCK_ROOT)) {
      throw { code: 'E_PERMISSION' };
    }
    const entries = MOCK_FS[stripTrailingSlashes(dir)];
    if (!entries) throw { code: 'E_NOT_FOUND' };
    return { path: dir, root: MOCK_ROOT, entries };
  }

  if (method === 'GET' && pathname === '/archives/info') {
    if (MOCK_FAIL) throw { code: 'E_MALFORMED_ARCHIVE' };
    return {
      path: query.get('path'),
      formatVersion: { major: 1, minor: 0 },
      fileCount: 3,
      blockCount: 700,
      originalBytes: 734016255,
      archiveBytes: 412000000,
      ratio: 1.78,
      storedBlocks: 41,
      compressedBlocks: 659,
      transformUsage: MOCK_USAGE,
      codecUsage: [{ id: 0, name: 'Raw', blocks: 41 }, { id: 2, name: 'Huffman', blocks: 659 }],
      files: [
        { path: 'README.md', originalBytes: 4210, blocks: 1 },
        { path: 'data.csv', originalBytes: 734000000, blocks: 698 },
        { path: 'src/main.cpp', originalBytes: 12045, blocks: 1 }
      ],
      filesTruncated: false
    };
  }

  if (method === 'POST') {
    mockJob = { operation: pathname.split('/').pop(), progress: 0, body };
    return { jobId: 'j-0001' };
  }

  // GET /jobs/{id}
  mockJob.progress = Math.min(1, mockJob.progress + 0.15);
  if (MOCK_FAIL && mockJob.progress >= 0.45) {
    return { state: 'failed', progress: mockJob.progress, error: { code: 'E_BLOCK_CHECKSUM' } };
  }
  if (mockJob.progress < 1) {
    return { state: mockJob.progress < 0.2 ? 'queued' : 'running', progress: mockJob.progress };
  }
  const base = {
    originalBytes: 734016255, archiveBytes: 412000000, ratio: 1.78,
    elapsedMs: 5210, throughputMBps: 134.5, files: 3, blocks: 700, stored: 41,
    transformUsage: MOCK_USAGE
  };
  if (mockJob.operation === 'verify') base.valid = true;
  return { state: 'completed', progress: 1, result: base };
}

// ------------------------------------------------------------ browser
//
// A file and folder picker backed by GET /api/fs/list (ADR-06: browsers do
// not expose real paths, so the backend lists its own allowed folder).

const browser = {
  kind: 'file',       // file | folder | file-or-folder | save-archive
  target: null,       // the input that receives the choice
  path: '',
  root: ''
};

function openBrowser(target, kind) {
  browser.kind = kind;
  browser.target = target;

  const titles = {
    'file': 'Choose an archive',
    'folder': 'Choose a folder',
    'file-or-folder': 'Choose a file or folder',
    'save-archive': 'Choose where to save the archive'
  };
  $('browser-title').textContent = titles[kind];
  $('browser-select').hidden = kind === 'file';
  $('browser-select').textContent =
    kind === 'save-archive' ? 'Save in this folder' : 'Use this folder';

  // Start next to whatever is already typed, else at the allowed root.
  const typed = target.value.trim();
  const start = typed ? (kind === 'folder' ? typed : dirname(typed)) : '';

  $('browser').showModal();
  loadFolder(start).catch(() => loadFolder('').catch(() => {}));
}

async function loadFolder(path) {
  $('browser-message').textContent = 'Loading…';
  $('browser-list').innerHTML = '';
  let listing;
  try {
    listing = await api('GET', '/fs/list' + (path ? '?path=' + encodeURIComponent(path) : ''));
  } catch (error) {
    $('browser-message').textContent = userMessageFor(error);
    throw error;
  }

  browser.path = listing.path;
  browser.root = listing.root || browser.root;
  $('browser-path').textContent = listing.path;
  $('browser-up').disabled = listing.path === browser.root;

  const entries = (listing.entries || []).filter(e =>
    e.type === 'directory' || browser.kind !== 'folder');
  // Folders first, then files, each alphabetical.
  entries.sort((a, b) =>
    (a.type === b.type ? 0 : a.type === 'directory' ? -1 : 1) ||
    a.name.localeCompare(b.name));

  $('browser-message').textContent = entries.length === 0
    ? 'This folder is empty.'
    : entries.length + (entries.length === 1 ? ' item' : ' items');

  for (const entry of entries) {
    const isDir = entry.type === 'directory';
    const li = document.createElement('li');
    const button = document.createElement('button');
    button.type = 'button';

    const kind = document.createElement('span');
    kind.className = 'kind';
    kind.textContent = isDir ? 'Folder' : 'File';
    const name = document.createElement('span');
    name.className = 'name';
    name.textContent = entry.name;
    const size = document.createElement('span');
    size.className = 'size';
    size.textContent = isDir ? '' : formatBytes(entry.size);
    button.append(kind, name, size);
    button.setAttribute('aria-label',
      (isDir ? 'Open folder ' : 'Choose file ') + entry.name);

    const full = joinPath(listing.path, entry.name);
    button.addEventListener('click', () => {
      if (isDir) {
        loadFolder(full)
          .then(() => $('browser-list').querySelector('button')?.focus())
          .catch(() => {});
      } else {
        choosePath(full);
      }
    });
    li.append(button);
    $('browser-list').append(li);
  }
}

function choosePath(path) {
  const target = browser.target;
  target.value = path;
  target.removeAttribute('aria-invalid');
  $('browser').close();
  target.focus();
  target.dispatchEvent(new Event('change'));
}

$('browser-up').addEventListener('click', () => {
  if (browser.path && browser.path !== browser.root) {
    loadFolder(dirname(browser.path)).catch(() => {});
  }
});
$('browser-select').addEventListener('click', () => {
  if (browser.kind === 'save-archive') {
    const source = $('source').value.trim();
    const name = (source ? basename(source) : 'archive') + '.uatc';
    choosePath(joinPath(browser.path, name));
  } else {
    choosePath(browser.path);
  }
});
$('browser-cancel').addEventListener('click', () => $('browser').close());

// ------------------------------------------------------------ select

function choose(op) {
  operation = op;
  const config = OPERATIONS[op];

  $('select-title').textContent = config.title;
  $('source-label').textContent = config.sourceLabel;
  $('btn-start').textContent = config.start;

  const needsDestination = Boolean(config.destinationLabel);
  $('destination-field').hidden = !needsDestination;
  if (needsDestination) {
    $('destination-label').textContent = config.destinationLabel;
    $('destination-help').textContent = config.destinationHelp;
  }
  $('advanced').hidden = op !== 'compress';
  $('verify-note').hidden = op !== 'verify';

  $('source').value = '';
  $('destination').value = '';
  clearFormError();
  show('select');
}

// Suggest a destination once a source is chosen (wireframes.md 2-3).
$('source').addEventListener('change', () => {
  const source = $('source').value.trim();
  if (!source || $('destination').value.trim()) return;
  if (operation === 'compress') {
    $('destination').value = stripTrailingSlashes(source) + '.uatc';
  } else if (operation === 'extract') {
    $('destination').value = joinPath(dirname(source),
      basename(source).replace(/\.uatc$/i, '') + '_restored');
  }
});

function clearFormError() {
  $('form-error').hidden = true;
  $('form-error').textContent = '';
  ['source', 'destination', 'workers'].forEach(id => $(id).removeAttribute('aria-invalid'));
}

function formError(field, text) {
  $('form-error').textContent = text;
  $('form-error').hidden = false;
  $(field).setAttribute('aria-invalid', 'true');
  $(field).focus();
}

// Validates the form; returns the request body or null.
function readForm() {
  clearFormError();
  const source = $('source').value.trim();
  const destination = $('destination').value.trim();

  if (!source) {
    formError('source', operation === 'compress'
      ? 'Please choose a file or folder to compress.'
      : 'Please choose an archive.');
    return null;
  }
  if (OPERATIONS[operation].destinationLabel && !destination) {
    formError('destination', 'Please choose a destination.');
    return null;
  }

  if (operation === 'compress') {
    const workers = Number($('workers').value);
    if (!Number.isInteger(workers) || workers < 1 || workers > 256) {
      $('advanced').open = true;
      formError('workers', 'Worker threads must be a whole number from 1 to 256.');
      return null;
    }
    return { source, destination, workers };
  }
  if (operation === 'extract') return { archive: source, destination };
  return { archive: source };
}

$('select-form').addEventListener('submit', event => {
  event.preventDefault();
  // start() handles its own errors (it shows the error screen), so the
  // returned promise is deliberately not awaited here.
  void start();
});
$('btn-browse-source').addEventListener('click', () =>
  openBrowser($('source'), OPERATIONS[operation].sourceKind));
$('btn-browse-destination').addEventListener('click', () =>
  openBrowser($('destination'), OPERATIONS[operation].destinationKind));

// ------------------------------------------------------------ run

async function start() {
  const body = readForm();
  if (!body) return;

  stopJob();

  // Archive Info is one synchronous call; no progress screen.
  if (operation === 'info') {
    $('btn-start').disabled = true;
    try {
      showInfo(await api('GET', '/archives/info?path=' + encodeURIComponent(body.archive)));
    } catch (error) {
      showError(error);
    } finally {
      $('btn-start').disabled = false;
    }
    return;
  }

  const generation = ++jobGeneration;
  setProgress('queued', 0);
  startedAt = Date.now();
  updateElapsed();
  elapsedTimer = setInterval(updateElapsed, 1000);
  show('progress');

  try {
    const response = await api('POST', '/jobs/' + operation, body);
    if (generation !== jobGeneration) return;
    poll(response.jobId, generation);
  } catch (error) {
    if (generation !== jobGeneration) return;
    stopJob();
    showError(error);
  }
}

function setProgress(state, progress) {
  const fraction = Math.max(0, Math.min(1, Number(progress) || 0));
  $('progress-bar').value = fraction;
  $('progress-text').textContent = Math.round(fraction * 100) + '%';
  $('progress-status').textContent = state === 'queued'
    ? 'Waiting to start…'
    : OPERATIONS[operation].working;
}

function updateElapsed() {
  $('progress-elapsed').textContent = 'Elapsed: ' + formatClock(Date.now() - startedAt);
}

// Polls with setTimeout, never setInterval, so a slow reply can never
// overlap the next request and the page stays responsive (NFR-08).
function poll(jobId, generation) {
  pollTimer = setTimeout(async () => {
    let job;
    try {
      job = await api('GET', '/jobs/' + encodeURIComponent(jobId));
    } catch (error) {
      if (generation !== jobGeneration) return;
      stopJob();
      showError(error);
      return;
    }
    if (generation !== jobGeneration) return;   // the user moved on

    if (job.state === 'queued' || job.state === 'running') {
      setProgress(job.state, job.progress);
      poll(jobId, generation);
      return;
    }

    stopJob();
    if (job.state === 'completed') {
      setProgress('running', 1);
      showResult(job.result || {});
    } else {
      showError(job.error);
    }
  }, POLL_MS);
}

function stopJob() {
  if (pollTimer !== null) clearTimeout(pollTimer);
  if (elapsedTimer !== null) clearInterval(elapsedTimer);
  pollTimer = null;
  elapsedTimer = null;
}

// ------------------------------------------------------------ results

function showResult(result) {
  if (operation === 'verify') return showVerification(result);
  if (operation === 'extract') return showExtraction(result);
  return showCompression(result);
}

// Wireframe 7.
function showCompression(r) {
  $('result-title').textContent = 'Done';
  const rows = [
    ['Original size', formatBytes(r.originalBytes)],
    ['Archive size', formatBytes(r.archiveBytes)],
    ['Compression ratio', formatRatio(r.ratio)],
    ['Time', formatSeconds(r.elapsedMs)],
    ['Speed', Number.isFinite(Number(r.throughputMBps))
      ? Number(r.throughputMBps).toFixed(1) + ' MB/s' : 'Unknown'],
    ['Blocks stored as-is', `${r.stored ?? 0} of ${r.blocks ?? 0}`]
  ];
  if (Number(r.files) > 1) rows.unshift(['Files', r.files]);

  $('result-body').innerHTML =
    mockNote('these are sample statistics; nothing was compressed.') +
    '<p class="status-ok">The archive was created.</p>' +
    '<h3>Compression statistics</h3>' + renderPairs(rows) +
    (Number(r.stored) > 0 ? `<p class="help">${escapeHtml(STORED_EXPLANATION)}</p>` : '') +
    renderUsage(r.transformUsage, 'Method used for each part');
  show('result');
}

// Wireframe 8 (left).
function showExtraction(r) {
  $('result-title').textContent = 'Done';
  $('result-body').innerHTML =
    mockNote('no files were extracted.') +
    '<p class="status-ok">Your files were restored, byte for byte.</p>' +
    renderPairs([
      ['Files restored', r.files ?? 'Unknown'],
      ['Total size', formatBytes(r.originalBytes)],
      ['Time', formatSeconds(r.elapsedMs)],
      ['Restored into', $('destination').value.trim()]
    ]);
  show('result');
}

// Wireframe 8 (right).
function showVerification(r) {
  const valid = r.valid === true;
  $('result-title').textContent = valid ? 'Archive is OK' : 'Verification failed';
  $('result-body').innerHTML =
    mockNote('no real archive was checked.') +
    `<p class="${valid ? 'status-ok' : 'status-bad'}">${valid
      ? 'Every part of the archive passed its integrity check.'
      : 'The archive did not pass its integrity check.'}</p>` +
    renderPairs([
      ['Files checked', r.files ?? 'Unknown'],
      ['Blocks checked', r.blocks ?? 'Unknown'],
      ['Original size', formatBytes(r.originalBytes)]
    ]) +
    '<p class="help">Nothing was written.</p>';
  show('result');
}

// Wireframe 4 (Info result). No time or speed: they are not stored in the
// archive (inspection-data.md section 2).
function showInfo(info) {
  $('result-title').textContent = 'Archive information';
  const version = info.formatVersion
    ? `${info.formatVersion.major}.${info.formatVersion.minor ?? 0}` : 'Unknown';
  const rows = [
    ['Archive file', `${basename(info.path || $('source').value)} (format ${version})`],
    ['Files', info.fileCount],
    ['Blocks', info.blockCount],
    ['Original size', formatBytes(info.originalBytes)],
    ['Archive size', formatBytes(info.archiveBytes)],
    ['Compression ratio', formatRatio(info.ratio)],
    ['Blocks stored as-is', `${info.storedBlocks ?? 0} of ${info.blockCount ?? 0}`]
  ];

  let files = '';
  if (Array.isArray(info.files) && info.files.length > 0) {
    files = `
      <details${info.files.length <= 10 ? ' open' : ''}>
        <summary>Files in this archive (${escapeHtml(info.fileCount)})</summary>
        <div class="table-wrap"><table>
          <thead><tr><th scope="col">File</th><th scope="col">Size</th></tr></thead>
          <tbody>${info.files.map(f => `
            <tr><td>${escapeHtml(f.path)}</td><td class="num">${escapeHtml(formatBytes(f.originalBytes))}</td></tr>`
          ).join('')}</tbody>
        </table></div>
        ${info.filesTruncated ? '<p class="help">Only the first files are listed.</p>' : ''}
      </details>`;
  }

  $('result-body').innerHTML =
    mockNote('these values were not read from a real archive.') +
    '<p class="help">Nothing was extracted.</p>' +
    renderPairs(rows) +
    (Number(info.storedBlocks) > 0 ? `<p class="help">${escapeHtml(STORED_EXPLANATION)}</p>` : '') +
    renderUsage(info.transformUsage, 'Method used for each part') +
    files;
  show('result');
}

// Wireframe 9: plain language only, never a code or a stack trace.
function showError(error) {
  $('error-text').textContent = userMessageFor(error);
  show('error');
}

// ------------------------------------------------------------ wiring

$('btn-compress').addEventListener('click', () => choose('compress'));
$('btn-extract').addEventListener('click', () => choose('extract'));
$('btn-info').addEventListener('click', () => choose('info'));
$('btn-verify').addEventListener('click', () => choose('verify'));

$('btn-back').addEventListener('click', () => show('home'));
$('btn-again').addEventListener('click', () => show('home'));
$('btn-progress-home').addEventListener('click', () => {
  // The job keeps running in the backend; we only stop watching it.
  jobGeneration++;
  stopJob();
  show('home');
});
$('btn-error-retry').addEventListener('click', () => show('select'));
$('btn-error-back').addEventListener('click', () => show('home'));

if (USE_MOCK) {
  $('mode-banner').hidden = false;
  $('mode-banner').textContent =
    'Demo mode: no files are read or written. Start `uatc serve` and open ' +
    'the address it prints to work with real files.';
} else if (!TOKEN) {
  $('mode-banner').hidden = false;
  $('mode-banner').textContent =
    'No session key in the address. Open the exact address `uatc serve` ' +
    'printed (it ends with #token=…).';
}
