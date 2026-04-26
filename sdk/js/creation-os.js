/**
 * Thin HTTP client for cos-serve (Node 18+ for global fetch).
 *
 * Usage:
 *   const { CreationOS } = require('./creation-os.js');
 *   const cos = new CreationOS();
 *   const r = await cos.gate('What is 2+2?');
 */

function defaultPort() {
  const p = process.env.COS_SERVE_PORT;
  return p ? parseInt(p, 10) : 3001;
}

class CreationOS {
  /**
   * @param {{ host?: string, port?: number, baseUrl?: string, timeoutMs?: number }} [opts]
   */
  constructor(opts = {}) {
    if (opts.baseUrl) {
      this.base = opts.baseUrl.replace(/\/$/, '');
    } else {
      const host = opts.host || 'localhost';
      const port = opts.port != null ? opts.port : defaultPort();
      this.base = `http://${host}:${port}`;
    }
    this.timeoutMs = opts.timeoutMs != null ? opts.timeoutMs : 600000;
  }

  async _json(method, path, bodyObj) {
    const ctrl = new AbortController();
    const id = setTimeout(() => ctrl.abort(), this.timeoutMs);
    try {
      const init = {
        method,
        headers: { 'Content-Type': 'application/json' },
        signal: ctrl.signal,
      };
      if (bodyObj != null) init.body = JSON.stringify(bodyObj);
      const res = await fetch(`${this.base}${path}`, init);
      const text = await res.text();
      let data;
      try {
        data = text ? JSON.parse(text) : {};
      } catch {
        throw new Error(`cos-serve: non-JSON response (${res.status}): ${text.slice(0, 200)}`);
      }
      if (!res.ok) {
        const err = new Error(data.error || `HTTP ${res.status}`);
        err.status = res.status;
        err.body = data;
        throw err;
      }
      return data;
    } finally {
      clearTimeout(id);
    }
  }

  async gate(prompt, model = 'gemma3:4b') {
    return this._json('POST', '/v1/gate', { prompt, model });
  }

  async verify(text, model = 'gemma3:4b') {
    return this._json('POST', '/v1/verify', { text, model });
  }

  async health() {
    return this._json('GET', '/v1/health', null);
  }

  async audit(auditId) {
    return this._json('GET', `/v1/audit/${encodeURIComponent(auditId)}`, null);
  }
}

module.exports = { CreationOS };
