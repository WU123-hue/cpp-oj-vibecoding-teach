/**
 * landing.js — 落地页逻辑
 * 检查登录状态、加载题目数量统计
 */

(function() {

  /**
   * 检查登录状态，切换顶部按钮
   */
  function checkAuth() {
    var userJson = localStorage.getItem('oj_user');
    var loggedIn = document.getElementById('loggedInActions');
    var loggedOut = document.getElementById('loggedOutActions');

    if (userJson) {
      try {
        var user = JSON.parse(userJson);
        document.getElementById('heroUserName').textContent =
          user.username || '用户';
        loggedIn.style.display = 'flex';
        loggedOut.style.display = 'none';
      } catch (e) {
        loggedIn.style.display = 'none';
        loggedOut.style.display = 'flex';
      }
    } else {
      loggedIn.style.display = 'none';
      loggedOut.style.display = 'flex';
    }
  }

  /**
   * 加载题目数量
   */
  async function loadStats() {
    var statEl = document.getElementById('statProblems');

    try {
      var resp = await API.getProblems();
      if (resp.ok && resp.data.code === 200) {
        var count = (resp.data.data || []).length;
        // 数字递增动画
        animateNumber(statEl, 0, count, 800);
      }
    } catch (err) {
      statEl.textContent = '0';
    }
  }

  /**
   * 数字递增动画
   */
  function animateNumber(el, from, to, duration) {
    var start = null;

    function step(timestamp) {
      if (!start) start = timestamp;
      var progress = Math.min((timestamp - start) / duration, 1);
      var value = Math.floor(from + (to - from) * progress);
      el.textContent = value;
      if (progress < 1) {
        requestAnimationFrame(step);
      } else {
        el.textContent = to;
      }
    }

    requestAnimationFrame(step);
  }

  // 初始化
  checkAuth();
  loadStats();

})();
