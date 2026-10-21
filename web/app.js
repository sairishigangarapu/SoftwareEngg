
/*
 * UATC web shell.
 * Mock mode is enabled by default.
 * No actual compression or extraction is performed in mock mode.
 */

const USE_MOCK = true;
const MOCK_FAIL = new URLSearchParams(location.search).has('fail');
const API = '/api';

const MESSAGES = {
  E_INVALID_ARGUMENT: 'Please check the selected file and input fields.',
  E_NOT_FOUND: 'We could not find that file or folder.',
  E_PERMISSION: 'UATC is not allowed to access that location.',
  E_IO: 'The disk could not be read or written.',
  E_DISK_FULL: 'The disk may be full.',
  E_MALFORMED_ARCHIVE: 'This file is not a valid UATC archive or is damaged.',
  E_UNSUPPORTED_VERSION: 'This archive uses an unsupported UATC version.',
  E_MISSING_BLOCK: 'Part of this archive is missing.',
  E_BLOCK_CHECKSUM: 'The archive contains a corrupted block.',
  E_ARCHIVE_CHECKSUM: 'The archive failed its integrity check.',
  E_PATH_TRAVERSAL: 'The archive contains an unsafe file path.',
  E_INTERNAL: 'Something went wrong inside UATC. Please try again.'
};

const MOCK_RESULT = {
  originalBytes: 734003200,
  archiveBytes: 412000000,
  ratio: 1.78,
  elapsedMs: 5210,
  throughputMBps: 134.5,
  blocks: 700,
  stored: 41,
  transformUsage: [
    { name: 'None', blocks: 300 },
    { name: 'Delta', blocks: 120 },
    { name: 'XOR', blocks: 30 },
    { name: 'Byte-shuffle', blocks: 50 },
    { name: 'RLE', blocks: 200 }
  ]
};

// Illustrative data for the frontend prototype only.
const MOCK_INFO = {
  archiveName: 'sample.uatc',
  version: 1,
  fileCount: 1,
  blockCount: 700,
  originalBytes: 734003200,
  archiveBytes: 412000000
};

const $ = id => document.getElementById(id);

let operation = 'compress';
let timer = null;
let mockProgress = 0;

function show(screenName) {
  document.querySelectorAll('.screen').forEach(screen => {
    screen.hidden = screen.id !== 'screen-' + screenName;
  });

  const heading = $('screen-' + screenName)?.querySelector('h2');
  if (heading) heading.focus();
}

function formatBytes(bytes) {
  const value = Number(bytes);

  if (!Number.isFinite(value) || value < 0) {
    return 'Unknown';
  }

  const units = ['B', 'KB', 'MB', 'GB', 'TB'];
  let size = value;
  let index = 0;

  while (size >= 1024 && index < units.length - 1) {
    size /= 1024;
    index++;
  }

  return size.toFixed(index === 0 ? 0 : 1) + ' ' + units[index];
}

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, character => ({
    '&': '&amp;',
    '<': '&lt;',
    '>': '&gt;',
    '"': '&quot;',
    "'": '&#39;'
  })[character]);
}

function renderTable(rows) {
  return `
    <table>
      <tbody>
        ${rows.map(([label, value]) => `
          <tr>
            <th>${escapeHtml(label)}</th>
            <td>${escapeHtml(value)}</td>
          </tr>
        `).join('')}
      </tbody>
    </table>
  `;
}

function mockApi(method, path) {
  // Mock archive-information endpoint.
  if (method === 'GET' && path.startsWith('/archives/info')) {
    return {
      archive: { ...MOCK_INFO },
      mock: true
    };
  }

  // Start a mock job.
  if (method === 'POST') {
    mockProgress = 0;
    return { jobId: 'j-0001' };
  }

  // Simulate a job-status request.
  mockProgress = Math.min(1, mockProgress + 0.2);

  if (MOCK_FAIL && mockProgress >= 0.6) {
    return {
      state: 'failed',
      error: {
        code: 'E_BLOCK_CHECKSUM',
        message: 'Simulated checksum failure',
        userMessage: MESSAGES.E_BLOCK_CHECKSUM
      }
    };
  }

  if (mockProgress < 1) {
    return {
      state: 'running',
      progress: mockProgress
    };
  }

  if (operation === 'verify') {
    return {
      state: 'completed',
      result: {
        valid: true,
        message: 'Demo verification passed. No real archive was checked.',
        mock: true
      }
    };
  }

  if (operation === 'extract') {
    return {
      state: 'completed',
      result: {
        message: 'Demo extraction completed. No files were extracted.',
        mock: true
      }
    };
  }

  return {
    state: 'completed',
    result: {
      ...MOCK_RESULT,
      mock: true
    }
  };
}

async function api(method, path, body) {
  if (USE_MOCK) {
    return mockApi(method, path);
  }

  const options = {
    method,
    headers: { 'Content-Type': 'application/json' }
  };

  if (body !== undefined) {
    options.body = JSON.stringify(body);
  }

  const response = await fetch(API + path, options);

  let data;

  try {
    data = await response.json();
  } catch {
    throw {
      code: 'E_INTERNAL',
      message: 'The server returned an invalid response.'
    };
  }

  if (!response.ok) {
    throw data.error || {
      code: 'E_INTERNAL',
      message: 'The request failed.'
    };
  }

  return data;
}

function showError(error) {
  $('error-text').textContent =
    error?.userMessage ||
    MESSAGES[error?.code] ||
    error?.message ||
    MESSAGES.E_INTERNAL;

  show('error');
}

function showInfo(response) {
  // Accept either { archive: {...} } or a direct metadata object.
  const info = response?.archive || response?.metadata || response;

  const rows = [
    ['Archive', info.archiveName || info.name || 'Unknown'],
    ['Version', info.version ?? 'Unknown'],
    ['Files', info.fileCount ?? 'Unknown'],
    ['Blocks', info.blockCount ?? 'Unknown'],
    ['Original size', formatBytes(info.originalBytes)],
    ['Archive size', formatBytes(info.archiveBytes)]
  ];

  $('screen-result').querySelector('h2').textContent = 'Archive Info';

  $('result-body').innerHTML =
    (response?.mock
      ? '<p><strong>Demo only:</strong> these values were not read from a real archive.</p>'
      : '') +
    '<h3>Archive Information</h3>' +
    renderTable(rows);

  show('result');
}

function showVerification(result) {
  const valid = result?.valid === true;

  $('screen-result').querySelector('h2').textContent =
    valid ? 'Verification Passed' : 'Verification Failed';

  $('result-body').innerHTML = `
    <h3>${valid ? 'Integrity check passed' : 'Integrity check failed'}</h3>
    <p>${escapeHtml(result?.message ||
      (valid ? 'Archive verification passed.' : 'Archive verification failed.'))}</p>
    ${result?.mock
      ? '<p><strong>Demo only:</strong> no real archive was verified.</p>'
      : ''}
  `;

  show('result');
}

function showResult(result) {
  if (operation === 'info') {
    showInfo(result);
    return;
  }

  if (operation === 'verify') {
    showVerification(result);
    return;
  }

  $('screen-result').querySelector('h2').textContent = 'Done';

  if (operation === 'extract') {
    $('result-body').innerHTML = `
      <h3>Extraction Result</h3>
      <p>${escapeHtml(result?.message || 'Extraction completed.')}</p>
      ${result?.mock
        ? '<p><strong>Demo only:</strong> no files were extracted.</p>'
        : ''}
    `;

    show('result');
    return;
  }

  const rows = [
    ['Original size', formatBytes(result.originalBytes)],
    ['Archive size', formatBytes(result.archiveBytes)],
    ['Compression ratio', Number(result.ratio).toFixed(2)],
    ['Time', (Number(result.elapsedMs) / 1000).toFixed(1) + ' s'],
    ['Speed', Number(result.throughputMBps).toFixed(1) + ' MB/s'],
    ['Blocks stored as-is', `${result.stored} of ${result.blocks}`]
  ];

  const transforms = (result.transformUsage || []).map(transform => `
    <tr>
      <td>${escapeHtml(transform.name)}</td>
      <td>${escapeHtml(transform.blocks)}</td>
    </tr>
  `).join('');

  $('result-body').innerHTML = `
    ${result.mock
      ? '<p><strong>Demo only:</strong> these are sample compression statistics.</p>'
      : ''}
    <h3>Compression Statistics</h3>
    ${renderTable(rows)}
    <h3>Transform Usage</h3>
    <table>
      <thead>
        <tr><th>Method</th><th>Blocks</th></tr>
      </thead>
      <tbody>${transforms}</tbody>
    </table>
  `;

  show('result');
}

function stopPolling() {
  if (timer !== null) {
    clearInterval(timer);
    timer = null;
  }
}

function poll(jobId) {
  stopPolling();

  timer = setInterval(async () => {
    try {
      const job = await api(
        'GET',
        '/jobs/' + encodeURIComponent(jobId)
      );

      if (job.state === 'running' || job.state === 'queued') {
        const progress = Number(job.progress) || 0;

        $('progress-bar').value = Math.max(0, Math.min(1, progress));
        $('progress-text').textContent =
          Math.round(progress * 100) + '%';

        return;
      }

      stopPolling();

      if (job.state === 'completed') {
        showResult(job.result || {});
      } else {
        showError(job.error || { code: 'E_INTERNAL' });
      }
    } catch (error) {
      stopPolling();
      showError(error);
    }
  }, 500);
}

async function start() {
  const source = $('source').value.trim();
  const destination = $('destination').value.trim();

  if (!source) {
    showError({
      code: 'E_INVALID_ARGUMENT',
      userMessage: 'Please enter a source file or archive path.'
    });
    return;
  }

  if (
    ['compress', 'extract'].includes(operation) &&
    !destination
  ) {
    showError({
      code: 'E_INVALID_ARGUMENT',
      userMessage: 'Please enter a destination path.'
    });
    return;
  }

  if (
    operation === 'compress' &&
    (!Number.isInteger(Number($('workers').value)) ||
      Number($('workers').value) < 1)
  ) {
    showError({
      code: 'E_INVALID_ARGUMENT',
      userMessage: 'Worker threads must be a positive integer.'
    });
    return;
  }

  stopPolling();

  // Archive Info uses a direct metadata request.
  if (operation === 'info') {
    try {
      const path =
        '/archives/info?path=' + encodeURIComponent(source);

      const result = await api('GET', path);
      showInfo(result);
    } catch (error) {
      showError(error);
    }

    return;
  }

  const body = operation === 'compress'
    ? {
        source,
        destination,
        workers: Number($('workers').value)
      }
    : operation === 'extract'
      ? { archive: source, destination }
      : { archive: source };

  $('progress-bar').value = 0;
  $('progress-text').textContent = '0%';

  show('progress');

  try {
    const response = await api(
      'POST',
      '/jobs/' + operation,
      body
    );

    if (response.error) {
      showError(response.error);
    } else if (response.jobId) {
      poll(response.jobId);
    } else if (response.state === 'completed') {
      showResult(response.result || {});
    } else if (response.state === 'failed') {
      showError(response.error || { code: 'E_INTERNAL' });
    } else {
      showError({
        code: 'E_INTERNAL',
        message: 'The server returned an unexpected job response.'
      });
    }
  } catch (error) {
    showError(error);
  }
}

function choose(op) {
  operation = op;

  const titles = {
    compress: 'Compress',
    extract: 'Extract',
    info: 'Archive Info',
    verify: 'Verify Archive'
  };

  $('select-title').textContent = titles[op];

  const needsDestination = ['compress', 'extract'].includes(op);

  const destinationLabel =
    document.querySelector('label[for="destination"]');

  if (destinationLabel) {
    destinationLabel.hidden = !needsDestination;
  }

  $('destination').hidden = !needsDestination;

  const workerDetails = $('workers').closest('details');

  if (workerDetails) {
    workerDetails.hidden = op !== 'compress';
  }

  $('source').value = '';
  $('destination').value = '';
  $('file-picker').value = '';

  show('select');
}

$('btn-compress').addEventListener('click', () => choose('compress'));
$('btn-extract').addEventListener('click', () => choose('extract'));
$('btn-info').addEventListener('click', () => choose('info'));
$('btn-verify').addEventListener('click', () => choose('verify'));

$('file-picker').addEventListener('change', event => {
  const file = event.target.files?.[0];

  if (file) {
    // This is only a filename, not the real local path.
    $('source').value = file.name;
  }
});

$('btn-start').addEventListener('click', start);
$('btn-back').addEventListener('click', () => {
  stopPolling();
  show('home');
});
$('btn-again').addEventListener('click', () => show('home'));
$('btn-error-back').addEventListener('click', () => {
  stopPolling();
  show('home');
});
