/**
 * api.js — API 调用封装
 * 统一封装所有 HTTP 请求，支持 Cookie 认证
 */

const API = (() => {

  async function request(method, path, body) {
    const opts = {
      method: method,
      headers: {},
      credentials: 'same-origin',
    };

    if (body !== undefined) {
      opts.headers['Content-Type'] = 'application/json';
      opts.body = JSON.stringify(body);
    }

    try {
      const resp = await fetch(path, opts);
      const data = await resp.json();
      return { ok: resp.ok, status: resp.status, data: data };
    } catch (err) {
      return {
        ok: false,
        status: 0,
        data: { code: 0, message: '网络错误：无法连接服务器' }
      };
    }
  }

  return {
    // 认证
    register: (username, password) =>
      request('POST', '/api/register', { username, password }),

    login: (username, password) =>
      request('POST', '/api/login', { username, password }),

    logout: () => request('POST', '/api/logout', {}),

    // 题目
    getProblems: () => request('GET', '/api/problems'),

    getProblem: (id) => request('GET', `/api/problems/${id}`),

    createProblem: (data) =>
      request('POST', '/api/admin/problems', data),

    deleteProblem: (id) =>
      request('DELETE', `/api/admin/problems/${id}`),

    // 代码提交
    submit: (problemId, code) =>
      request('POST', '/api/submit', { problem_id: problemId, code: code }),
  };
})();
