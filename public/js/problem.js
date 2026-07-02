/**
 * problem.js — 题目列表页逻辑
 * 加载题目列表、难度筛选、渲染表格、处理导航
 */

(function() {

  // DOM 元素
  var tbody = document.getElementById('problemTableBody');
  var headRow = document.getElementById('problemTableHead');
  var emptyState = document.getElementById('emptyState');
  var countEl = document.getElementById('problemCount');
  var userName = document.getElementById('userName');
  var userBadge = document.getElementById('userBadge');
  var adminLink = document.getElementById('adminLink');
  var logoutLink = document.getElementById('logoutLink');
  var filterTabs = document.querySelectorAll('.filter-tab');

  // 全部题目数据（缓存在内存中，筛选时不再重新请求）
  var allProblems = [];
  // 当前筛选条件
  var currentFilter = 'all';

  // 难度样式映射
  var diffClass = {
    Easy: 'difficulty--easy',
    Medium: 'difficulty--medium',
    Hard: 'difficulty--hard',
  };

  // 难度中文映射
  var diffLabel = {
    Easy: '简单',
    Medium: '中等',
    Hard: '困难',
  };

  /**
   * 显示加载骨架屏
   */
  function showSkeleton() {
    tbody.innerHTML = '';
    for (var i = 0; i < 5; i++) {
      var row = document.createElement('div');
      row.className = 'skeleton-row';
      row.innerHTML =
        '<div class="skeleton skeleton--sm"></div>' +
        '<div class="skeleton skeleton--md"></div>' +
        '<div class="skeleton skeleton--xs"></div>' +
        '<div></div>';
      tbody.appendChild(row);
    }
  }

  /**
   * 根据当前筛选条件渲染题目列表
   */
  function renderProblems() {
    var problems = allProblems;

    // 筛选
    if (currentFilter !== 'all') {
      problems = problems.filter(function(p) {
        return p.difficulty === currentFilter;
      });
    }

    // 更新计数
    countEl.querySelector('span').textContent = problems.length;

    if (!problems || problems.length === 0) {
      headRow.style.display = 'none';
      emptyState.style.display = 'flex';
      tbody.innerHTML = '';
      return;
    }

    headRow.style.display = 'grid';
    emptyState.style.display = 'none';

    tbody.innerHTML = '';
    for (var i = 0; i < problems.length; i++) {
      var p = problems[i];
      var row = document.createElement('a');
      row.className = 'problem-row';
      row.href = '/problem.html?id=' + p.id;

      var diff = p.difficulty || 'Easy';
      var cls = diffClass[diff] || 'difficulty--easy';
      var label = diffLabel[diff] || diff;

      row.innerHTML =
        '<span class="problem-row__id">#' + p.id + '</span>' +
        '<span class="problem-row__title">' + escapeHtml(p.title) + '</span>' +
        '<span class="problem-row__difficulty ' + cls + '">' +
          '<span class="difficulty-dot"></span>' + label +
        '</span>' +
        '<span class="problem-row__action">' +
          '<svg fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="2">' +
            '<path stroke-linecap="round" stroke-linejoin="round" d="M9 5l7 7-7 7" />' +
          '</svg>' +
        '</span>';

      tbody.appendChild(row);
    }
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
   * 加载题目列表
   */
  async function loadProblems() {
    showSkeleton();
    var resp = await API.getProblems();

    if (resp.ok && resp.data.code === 200) {
      allProblems = resp.data.data || [];
    } else {
      allProblems = [];
    }
    renderProblems();
  }

  /**
   * 切换筛选条件
   */
  function setFilter(filter) {
    currentFilter = filter;

    // 更新 tab 激活态
    filterTabs.forEach(function(tab) {
      if (tab.dataset.filter === filter) {
        tab.classList.add('filter-tab--active');
      } else {
        tab.classList.remove('filter-tab--active');
      }
    });

    renderProblems();
  }

  /**
   * 用户信息 — 从 localStorage 获取登录时保存的信息
   */
  function loadUserInfo() {
    var userJson = localStorage.getItem('oj_user');
    if (!userJson) {
      window.location.href = '/login.html';
      return;
    }

    try {
      var user = JSON.parse(userJson);
      userName.textContent = user.username || '用户';

      var avatar = document.getElementById('userAvatar');
      if (avatar && user.username) {
        avatar.textContent = user.username.charAt(0).toUpperCase();
      }

      if (user.role === 'admin') {
        userBadge.textContent = 'admin';
        userBadge.style.display = 'inline-flex';
        adminLink.style.display = 'inline-flex';
      } else {
        userBadge.style.display = 'none';
        adminLink.style.display = 'none';
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
  loadProblems();

  // 筛选 tab 点击事件
  filterTabs.forEach(function(tab) {
    tab.addEventListener('click', function() {
      setFilter(tab.dataset.filter);
    });
  });

  // 退出
  logoutLink.addEventListener('click', function(e) {
    e.preventDefault();
    doLogout();
  });

})();
