/**
 * problem_detail.js — 题目详情页逻辑
 * 加载题目详情、初始化 Ace 编辑器、提交代码、展示结果
 */

(function() {

  var editor = null;
  var currentProblemId = null;

  // 难度映射
  var diffClass = {
    Easy: 'difficulty--easy',
    Medium: 'difficulty--medium',
    Hard: 'difficulty--hard',
  };

  var diffLabel = {
    Easy: '简单',
    Medium: '中等',
    Hard: '困难',
  };

  // 结果状态映射
  var statusClass = {
    AC: 'result-status--ac',
    WA: 'result-status--wa',
    CE: 'result-status--ce',
    TLE: 'result-status--tle',
    RE: 'result-status--re',
  };

  var statusText = {
    AC: 'Accepted',
    WA: 'Wrong Answer',
    CE: 'Compile Error',
    TLE: 'Time Limit Exceeded',
    RE: 'Runtime Error',
  };

  var statusDot = {
    AC: 'testcase-dot--ac',
    WA: 'testcase-dot--wa',
    TLE: 'testcase-dot--tle',
    RE: 'testcase-dot--re',
    CE: 'testcase-dot--wa',
  };

  var statusIcon = {
    AC: '<path stroke-linecap="round" stroke-linejoin="round" d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z"/>',
    WA: '<path stroke-linecap="round" stroke-linejoin="round" d="M10 14l2-2m0 0l2-2m-2 2l-2-2m2 2l2 2m7-2a9 9 0 11-18 0 9 9 0 0118 0z"/>',
    CE: '<path stroke-linecap="round" stroke-linejoin="round" d="M12 9v2m0 4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z"/>',
    TLE: '<path stroke-linecap="round" stroke-linejoin="round" d="M12 8v4l3 3m6-3a9 9 0 11-18 0 9 9 0 0118 0z"/>',
    RE: '<path stroke-linecap="round" stroke-linejoin="round" d="M12 9v2m0 4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z"/>',
  };

  /**
   * 从 URL 获取题目 ID
   */
  function getProblemId() {
    var params = new URLSearchParams(window.location.search);
    return params.get('id');
  }

  /**
   * 初始化 Ace 编辑器
   */
  function initEditor(template) {
    editor = ace.edit('editor');
    editor.setTheme('ace/theme/tomorrow_night_blue');
    editor.session.setMode('ace/mode/c_cpp');
    editor.setOptions({
      fontSize: '14px',
      fontFamily: "'JetBrains Mono', monospace",
      showPrintMargin: false,
      highlightActiveLine: true,
      tabSize: 4,
      useSoftTabs: true,
      wrap: true,
      enableLiveAutocompletion: true,
    });
    editor.renderer.setScrollMargin(8, 8, 0, 0);

    if (template) {
      editor.setValue(template, -1);
    } else {
      editor.setValue('#include <iostream>\nusing namespace std;\n\nint main() {\n    \n    return 0;\n}\n', -1);
    }
    editor.clearSelection();
  }

  /**
   * 加载题目详情
   */
  async function loadProblem(id) {
    var resp = await API.getProblem(id);

    if (!resp.ok || resp.data.code !== 200) {
      document.getElementById('detailContent').textContent =
        resp.data.message || '题目不存在';
      return;
    }

    var p = resp.data.data;
    currentProblemId = p.id;

    // 渲染头部
    document.getElementById('problemId').textContent = '#' + p.id;
    document.getElementById('problemTitle').textContent = p.title;

    var diffEl = document.getElementById('problemDifficulty');
    var diff = p.difficulty || 'Easy';
    diffEl.className = 'problem-row__difficulty ' + (diffClass[diff] || 'difficulty--easy');
    diffEl.innerHTML = '<span class="difficulty-dot"></span>' + (diffLabel[diff] || diff);

    // 渲染题目内容
    document.getElementById('detailContent').textContent = p.content || '';

    // 渲染元信息
    document.getElementById('metaTime').textContent = p.created_at || '-';
    document.getElementById('metaCases').textContent = (p.test_cases || []).length;

    // 初始化编辑器
    initEditor(p.template);
  }

  /**
   * 提交代码
   */
  async function submitCode() {
    if (!currentProblemId) return;

    var code = editor.getValue();
    if (!code.trim()) {
      showResultError('代码不能为空');
      return;
    }

    // 显示加载
    var overlay = document.getElementById('submitOverlay');
    overlay.classList.add('submit-overlay--visible');

    // 隐藏旧结果
    document.getElementById('resultPanel').classList.remove('result-panel--visible');

    try {
      var resp = await API.submit(currentProblemId, code);

      if (!resp.ok || resp.data.code !== 200) {
        showResultError(resp.data.message || '提交失败');
        return;
      }

      renderResult(resp.data.data);
    } catch (err) {
      showResultError('网络错误，请重试');
    } finally {
      overlay.classList.remove('submit-overlay--visible');
    }
  }

  /**
   * 渲染结果
   */
  function renderResult(data) {
    var panel = document.getElementById('resultPanel');
    var statusEl = document.getElementById('resultStatus');
    var statusIconEl = document.getElementById('resultStatusIcon');
    var statsEl = document.getElementById('resultStats');
    var compileEl = document.getElementById('compileOutput');
    var casesEl = document.getElementById('testcaseList');

    var status = data.status || 'RE';

    // 状态
    statusEl.className = 'result-status ' + (statusClass[status] || 'result-status--re');
    statusEl.firstChild.textContent = statusText[status] || status;
    statusIconEl.innerHTML = '<svg fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="2">' +
      (statusIcon[status] || statusIcon.RE) + '</svg>';

    // 统计
    statsEl.innerHTML =
      '<div class="result-stat"><span class="result-stat__value">' + data.passed + '/' + data.total + '</span><span class="result-stat__label">通过</span></div>' +
      '<div class="result-stat"><span class="result-stat__value">' + data.max_time_ms + 'ms</span><span class="result-stat__label">最大耗时</span></div>';

    // 编译输出
    if (status === 'CE' && data.compile_output) {
      compileEl.style.display = 'block';
      compileEl.textContent = data.compile_output;
    } else {
      compileEl.style.display = 'none';
    }

    // 测试用例
    var cases = data.cases || [];
    if (cases.length > 0) {
      casesEl.style.display = 'block';
      casesEl.innerHTML = '';
      for (var i = 0; i < cases.length; i++) {
        var c = cases[i];
        var item = document.createElement('div');
        item.className = 'testcase-item';

        var dotCls = statusDot[c.status] || 'testcase-dot--wa';
        var label = '用例 #' + (i + 1) + ' ' + c.status;
        var time = c.time_ms + 'ms';

        item.innerHTML =
          '<span class="testcase-dot ' + dotCls + '"></span>' +
          '<span class="testcase-label">' + escapeHtml(label) + '</span>' +
          '<span class="testcase-time">' + time + '</span>';
        casesEl.appendChild(item);
      }
    } else {
      casesEl.style.display = 'none';
    }

    panel.classList.add('result-panel--visible');
    panel.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
  }

  /**
   * 显示结果错误
   */
  function showResultError(msg) {
    var panel = document.getElementById('resultPanel');
    var statusEl = document.getElementById('resultStatus');
    var statusIconEl = document.getElementById('resultStatusIcon');
    var statsEl = document.getElementById('resultStats');
    var compileEl = document.getElementById('compileOutput');
    var casesEl = document.getElementById('testcaseList');

    statusEl.className = 'result-status result-status--re';
    statusEl.firstChild.textContent = 'Error';
    statusIconEl.innerHTML = '<svg fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="2"><path stroke-linecap="round" stroke-linejoin="round" d="M12 9v2m0 4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z"/></svg>';
    statsEl.innerHTML = '';
    compileEl.style.display = 'block';
    compileEl.textContent = msg;
    casesEl.style.display = 'none';

    panel.classList.add('result-panel--visible');
  }

  /**
   * HTML 转义
   */
  function escapeHtml(str) {
    var div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
  }

  /**
   * 加载用户信息
   */
  function loadUserInfo() {
    var userJson = localStorage.getItem('oj_user');
    if (!userJson) {
      window.location.href = '/login.html';
      return;
    }

    try {
      var user = JSON.parse(userJson);
      document.getElementById('userName').textContent = user.username || '用户';

      var avatar = document.getElementById('userAvatar');
      if (avatar && user.username) {
        avatar.textContent = user.username.charAt(0).toUpperCase();
      }

      var badge = document.getElementById('userBadge');
      var adminLink = document.getElementById('adminLink');
      if (user.role === 'admin') {
        badge.textContent = 'admin';
        badge.style.display = 'inline-block';
        if (adminLink) adminLink.style.display = 'inline-flex';
      } else {
        badge.style.display = 'none';
        if (adminLink) adminLink.style.display = 'none';
      }
    } catch (e) {
      window.location.href = '/login.html';
    }
  }

  /**
   * 登出
   */
  async function doLogout() {
    await API.logout();
    localStorage.removeItem('oj_user');
    window.location.href = '/login.html';
  }

  // ========== 初始化 ==========

  loadUserInfo();

  var pid = getProblemId();
  if (!pid) {
    document.getElementById('detailContent').textContent = '缺少题目 ID';
  } else {
    loadProblem(pid);
  }

  // 提交按钮
  document.getElementById('submitBtn').addEventListener('click', submitCode);

  // Ctrl+Enter 提交
  document.addEventListener('keydown', function(e) {
    if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
      e.preventDefault();
      submitCode();
    }
  });

  // 退出
  document.getElementById('logoutLink').addEventListener('click', function(e) {
    e.preventDefault();
    doLogout();
  });

})();
