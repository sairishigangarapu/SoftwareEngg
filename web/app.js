// UATC web shell. Presentation only: no compression logic lives here.
// USE_MOCK = true runs without a backend. Add ?fail to the URL to see the error screen.
const USE_MOCK = true;
const MOCK_FAIL = location.search.includes('fail');
const API = '/api';

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

const MOCK_RESULT = {
  originalBytes: 734003200, archiveBytes: 412000000, ratio: 1.78,
  elapsedMs: 5210, throughputMBps: 134.5, blocks: 700, stored: 41,
  transformUsage: [
    { id: 0, name: 'None', blocks: 300 }, { id: 1, name: 'Delta', blocks: 120 },
    { id: 2, name: 'XOR', blocks: 30 }, { id: 3, name: 'Byte-shuffle', blocks: 50 },
    { id: 4, name: 'RLE', blocks: 200 }
  ]
};

const $ = id => document.getElementById(id);
let operation = 'compress';
let timer = null;
let mockProgress = 0;

function show(name) {
  document.querySelectorAll('.screen').forEach(s => { s.hidden = s.id !== 'screen-' + name; });
  $('screen-' + name).querySelector('h2').focus();
}

function formatBytes(n) {
  const units = ['B', 'KB', 'MB', 'GB'];
  let i = 0;
  while (n >= 1024 && i < units.length - 1) { n /= 1024; i++; }
  return n.toFixed(i ? 1 : 0) + ' ' + units[i];
}

function mockApi(method) {
  if (method === 'POST') { mockProgress = 0; return { jobId: 'j-0001' }; }
  mockProgress = Math.min(1, mockProgress + 0.2);
  if (MOCK_FAIL && mockProgress >= 0.6) {
    return { state: 'failed', error: { code: 'E_BLOCK_CHECKSUM', message: 'block 213 CRC32 mismatch',
             userMessage: MESSAGES.E_BLOCK_CHECKSUM, details: { block: 213 } } };
  }
  return mockProgress < 1 ? { state: 'running', progress: mockProgress }
                          : { state: 'completed', progress: 1, result: MOCK_RESULT };
}

async function api(method, path, body) {
  if (USE_MOCK) return mockApi(method);
  const res = await fetch(API + path, {
    method, headers: { 'Content-Type': 'application/json' },
    body: body ? JSON.stringify(body) : undefined
  });
  return res.json();
}

function showError(error) {
  $('error-text').textContent = (error && error.userMessage) || MESSAGES[error && error.code] || MESSAGES.E_INTERNAL;
  show('error');
}

function showResult(r) {
  const rows = r.transformUsage.map(t => `<tr><td>${t.name}</td><td>${t.blocks}</td></tr>`).join('');
  $('result-body').innerHTML =
    `<table><tbody>
      <tr><th>Original size</th><td>${formatBytes(r.originalBytes)}</td></tr>
      <tr><th>Archive size</th><td>${formatBytes(r.archiveBytes)}</td></tr>
      <tr><th>Ratio</th><td>${r.ratio.toFixed(2)}</td></tr>
      <tr><th>Time</th><td>${(r.elapsedMs / 1000).toFixed(1)} s</td></tr>
      <tr><th>Speed</th><td>${r.throughputMBps.toFixed(1)} MB/s</td></tr>
      <tr><th>Blocks stored as-is</th><td>${r.stored} of ${r.blocks}</td></tr>
    </tbody></table>
    <table><thead><tr><th>Method used</th><th>Blocks</th></tr></thead><tbody>${rows}</tbody></table>`;
  show('result');
}

function poll(jobId) {
  timer = setInterval(async () => {
    const job = await api('GET', '/jobs/' + jobId);
    if (job.state === 'running' || job.state === 'queued') {
      $('progress-bar').value = job.progress || 0;
      $('progress-text').textContent = Math.round((job.progress || 0) * 100) + '%';
    } else {
      clearInterval(timer);
      job.state === 'completed' ? showResult(job.result) : showError(job.error);
    }
  }, 500);
}

async function start() {
  const body = operation === 'compress'
    ? { source: $('source').value, destination: $('destination').value, workers: Number($('workers').value) }
    : { archive: $('source').value, destination: $('destination').value };
  $('progress-bar').value = 0;
  $('progress-text').textContent = '0%';
  show('progress');
  try {
    const { jobId, error } = await api('POST', '/jobs/' + operation, body);
    error ? showError(error) : poll(jobId);
  } catch (e) {
    showError({ code: 'E_INTERNAL' });
  }
}

function choose(op) {
  operation = op;
  $('select-title').textContent = op === 'compress' ? 'Compress' : 'Extract';
  show('select');
}

$('btn-compress').addEventListener('click', () => choose('compress'));
$('btn-extract').addEventListener('click', () => choose('extract'));
$('btn-start').addEventListener('click', start);
$('btn-back').addEventListener('click', () => show('home'));
$('btn-again').addEventListener('click', () => show('home'));
$('btn-error-back').addEventListener('click', () => show('home'));
