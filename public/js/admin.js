/**
 * admin.js — 管理后台逻辑
 * 新增题目（含测试用例）、删除题目、题目列表管理
 */

(function() {

  // DOM 元素
  var form = document.getElementById('problemForm');
  var titleInput = document.getElementById('title');
  var difficultySelect = document.getElementById('difficulty');
  var contentInput = document.getElementById('content');
  var templateInput = document.getElementById('template');
  var testcaseEditor = document.getElementById('testcaseEditor');
  var addTcBtn = document.getElementById('addTcBtn');
  var submitBtn = document.getElementById('submitBtn');
  var adminTableBody = document.getElementById('adminTableBody');
  var adminTableHead = document.getElementById('adminTableHead');
  var adminEmpty = document.getElementById('adminEmpty');
  var logoutLink = document.getElementById('logoutLink');
  var toast = document.getElementById('toast');
  var dialog = document.getElementById('confirmDialog');
  var dialogConfirm = document.getElementById('dialogConfirm');
  var dialogCancel = document.getElementById('dialogCancel');

  // 待删除的题目 ID
  var pendingDeleteId = null;

  // 难度样式映射
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

  /**
   * 添加一行测试用例
   */
  function addTestCase(input, expected) {
    var container = document.createElement('div');
    container.className = 'testcase-row';

    var index = document.createElement('span');
    index.className = 'testcase-row__index';
    index.textContent = '#' + (testcaseEditor.children.length + 1);

    var inputEl = document.createElement('textarea');
    inputEl.className = 'testcase-input';
    inputEl.placeholder = '输入数据';
    inputEl.rows = 2;
    if (input) inputEl.value = input;

    var expectedEl = document.createElement('textarea');
    expectedEl.className = 'testcase-input';
    expectedEl.placeholder = '期望输出';
    expectedEl.rows = 2;
    if (expected) expectedEl.value = expected;

    var removeBtn = document.createElement('button');
    removeBtn.type = 'button';
    removeBtn.className = 'testcase-remove';
    removeBtn.innerHTML =
      '<svg fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="2">' +
        '<path stroke-linecap="round" stroke-linejoin="round" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" />' +
      '</svg>';
    removeBtn.addEventListener('click', function() {
      container.remove();
      updateTestCaseIndices();
    });

    container.appendChild(index);
    container.appendChild(inputEl);
    container.appendChild(expectedEl);
    container.appendChild(removeBtn);
    testcaseEditor.appendChild(container);
  }

  /**
   * 更新测试用例序号
   */
  function updateTestCaseIndices() {
    var rows = testcaseEditor.children;
    for (var i = 0; i < rows.length; i++) {
      var indexEl = rows[i].querySelector('.testcase-row__index');
      if (indexEl) indexEl.textContent = '#' + (i + 1);
    }
  }

  /**
   * 收集测试用例数据
   */
  function collectTestCases() {
    var cases = [];
    var rows = testcaseEditor.children;
    for (var i = 0; i < rows.length; i++) {
      var inputs = rows[i].querySelectorAll('.testcase-input');
      if (inputs.length >= 2) {
        cases.push({
          input: inputs[0].value,
          expected: inputs[1].value,
        });
      }
    }
    return cases;
  }

  /**
   * 提交新增题目
   */
  async function submitProblem() {
    var title = titleInput.value.trim();
    var difficulty = difficultySelect.value;
    var content = contentInput.value.trim();
    var template = templateInput.value;
    var testCases = collectTestCases();

    // 校验
    if (!title) {
      showToast('请输入题目标题', 'error');
      titleInput.focus();
      return;
    }
    if (!content) {
      showToast('请输入题目描述', 'error');
      contentInput.focus();
      return;
    }
    if (difficulty !== 'Easy' && difficulty !== 'Medium' && difficulty !== 'Hard') {
      showToast('请选择难度', 'error');
      return;
    }

    // 按钮 loading
    submitBtn.disabled = true;
    submitBtn.innerHTML = '<span class="spinner"></span>提交中...';

    try {
      var resp = await API.createProblem({
        title: title,
        difficulty: difficulty,
        content: content,
        template: template,
        test_cases: testCases,
      });

      if (resp.ok && resp.data.code === 200) {
        showToast('题目创建成功', 'success');
        resetForm();
        loadProblems();
      } else {
        showToast(resp.data.message || '创建失败', 'error');
      }
    } catch (err) {
      showToast('网络错误，请重试', 'error');
    } finally {
      submitBtn.disabled = false;
      submitBtn.innerHTML =
        '<svg fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="2" style="width:16px;height:16px;">' +
          '<path stroke-linecap="round" stroke-linejoin="round" d="M12 4v16m8-8H4" />' +
        '</svg>创建题目';
    }
  }

  /**
   * 重置表单
   */
  function resetForm() {
    form.reset();
    testcaseEditor.innerHTML = '';
    addTestCase();
  }

  /**
   * 加载题目列表
   */
  async function loadProblems() {
    adminTableBody.innerHTML = '';
    for (var i = 0; i < 3; i++) {
      var sk = document.createElement('div');
      sk.className = 'skeleton-row';
      sk.style.gridTemplateColumns = '60px 1fr 90px 90px 80px';
      sk.innerHTML =
        '<div class="skeleton skeleton--sm"></div>' +
        '<div class="skeleton skeleton--md"></div>' +
        '<div class="skeleton skeleton--xs"></div>' +
        '<div class="skeleton skeleton--xs"></div>' +
        '<div></div>';
      adminTableBody.appendChild(sk);
    }

    var resp = await API.getProblems();

    if (!resp.ok || resp.data.code !== 200) {
      adminTableBody.innerHTML = '';
      adminTableHead.style.display = 'none';
      adminEmpty.style.display = 'flex';
      return;
    }

    var list = resp.data.data || [];

    if (list.length === 0) {
      adminTableBody.innerHTML = '';
      adminTableHead.style.display = 'none';
      adminEmpty.style.display = 'flex';
      return;
    }

    adminTableHead.style.display = 'grid';
    adminEmpty.style.display = 'none';
    adminTableBody.innerHTML = '';

    for (var i = 0; i < list.length; i++) {
      var p = list[i];
      var row = document.createElement('div');
      row.className = 'admin-row';

      var diff = p.difficulty || 'Easy';
      var cls = diffClass[diff] || 'difficulty--easy';
      var label = diffLabel[diff] || diff;

      var dateStr = '-';
      if (p.created_at) {
        dateStr = p.created_at.substring(0, 10);
      }

      row.innerHTML =
        '<span class="admin-row__id">' + p.id + '</span>' +
        '<span class="admin-row__title"><a href="/problem.html?id=' + p.id + '" target="_blank">' +
          escapeHtml(p.title) +
        '</a></span>' +
        '<span class="problem-row__difficulty ' + cls + '">' +
          '<span class="difficulty-dot"></span>' + label +
        '</span>' +
        '<span class="admin-row__date">' + dateStr + '</span>' +
        '<span class="admin-row__actions">' +
          '<button type="button" class="icon-btn icon-btn--danger" data-delete-id="' + p.id + '">' +
            '<svg fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="2">' +
              '<path stroke-linecap="round" stroke-linejoin="round" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" />' +
            '</svg>' +
          '</button>' +
        '</span>';

      adminTableBody.appendChild(row);
    }

    // 绑定删除按钮
    var deleteBtns = adminTableBody.querySelectorAll('[data-delete-id]');
    deleteBtns.forEach(function(btn) {
      btn.addEventListener('click', function() {
        pendingDeleteId = btn.dataset.deleteId;
        showDialog();
      });
    });
  }

  /**
   * 删除题目
   */
  async function doDelete(id) {
    var resp = await API.deleteProblem(id);

    if (resp.ok && resp.data.code === 200) {
      showToast('删除成功', 'success');
      loadProblems();
    } else {
      showToast(resp.data.message || '删除失败', 'error');
    }
  }

  /**
   * 显示确认对话框
   */
  function showDialog() {
    dialog.classList.add('dialog-overlay--visible');
  }

  /**
   * 隐藏确认对话框
   */
  function hideDialog() {
    dialog.classList.remove('dialog-overlay--visible');
    pendingDeleteId = null;
  }

  /**
   * 显示 Toast
   */
  var toastTimer = null;
  function showToast(message, type) {
    clearTimeout(toastTimer);
    toast.className = 'toast toast--' + type;

    var icon = type === 'success'
      ? '<svg fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="2"><path stroke-linecap="round" stroke-linejoin="round" d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z"/></svg>'
      : '<svg fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="2"><path stroke-linecap="round" stroke-linejoin="round" d="M12 9v2m0 4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z"/></svg>';

    toast.innerHTML = icon + '<span>' + escapeHtml(message) + '</span>';

    requestAnimationFrame(function() {
      toast.classList.add('toast--visible');
    });

    toastTimer = setTimeout(function() {
      toast.classList.remove('toast--visible');
    }, 2500);
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
   * 用户信息
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
      if (user.role === 'admin') {
        badge.textContent = 'admin';
        badge.style.display = 'inline-flex';
      } else {
        // 非管理员跳转
        window.location.href = '/problem_list.html';
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
  addTestCase();
  loadProblems();

  // 添加测试用例
  addTcBtn.addEventListener('click', function() {
    addTestCase();
  });

  // 表单提交
  form.addEventListener('submit', function(e) {
    e.preventDefault();
    submitProblem();
  });

  // 对话框
  dialogConfirm.addEventListener('click', function() {
    if (pendingDeleteId) {
      doDelete(pendingDeleteId);
    }
    hideDialog();
  });

  dialogCancel.addEventListener('click', hideDialog);

  dialog.addEventListener('click', function(e) {
    if (e.target === dialog) hideDialog();
  });

  // 退出
  logoutLink.addEventListener('click', function(e) {
    e.preventDefault();
    doLogout();
  });

})();
