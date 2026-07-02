/**
 * auth.js — 认证状态管理
 * 提供 UI 工具函数：alert 显示、密码强度检测、登录态检查
 */

const Auth = (() => {

  /**
   * 显示 alert 消息
   * @param {string} elementId - alert 元素 ID
   * @param {string} message - 消息内容
   * @param {string} type - 'error' | 'success'
   */
  function showAlert(elementId, message, type) {
    const el = document.getElementById(elementId);
    if (!el) return;

    el.className = `alert alert--${type}`;
    const icon = type === 'error'
      ? '<svg fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 9v2m0 4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z"/></svg>'
      : '<svg fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z"/></svg>';
    el.innerHTML = icon + '<span>' + escapeHtml(message) + '</span>';
  }

  /**
   * 隐藏 alert
   */
  function hideAlert(elementId) {
    const el = document.getElementById(elementId);
    if (el) el.className = 'alert';
  }

  /**
   * 密码强度检测
   * @param {string} password
   * @returns {{level: 'weak'|'medium'|'strong', label: string}}
   */
  function passwordStrength(password) {
    if (!password || password.length < 6) {
      return { level: 'weak', label: '密码至少6位' };
    }

    let score = 0;
    if (password.length >= 8) score++;
    if (password.length >= 12) score++;
    if (/[A-Z]/.test(password)) score++;
    if (/[0-9]/.test(password)) score++;
    if (/[^A-Za-z0-9]/.test(password)) score++;

    if (score >= 4) return { level: 'strong', label: '密码强度：强' };
    if (score >= 2) return { level: 'medium', label: '密码强度：中' };
    return { level: 'weak', label: '密码强度：弱' };
  }

  /**
   * 更新密码强度 UI
   */
  function updateStrengthUI(strengthBarId, strengthLabelId, password) {
    const bar = document.getElementById(strengthBarId);
    const label = document.getElementById(strengthLabelId);
    if (!bar || !label) return;

    const { level, label: text } = passwordStrength(password);

    bar.className = 'strength-bar__fill';
    if (password.length === 0) {
      bar.style.width = '0';
      label.textContent = '';
    } else {
      bar.classList.add(`strength-bar__fill--${level}`);
      bar.style.width = '';
      label.textContent = text;
    }
  }

  /**
   * 设置按钮 loading 状态
   */
  function setLoading(btnId, loading, loadingText) {
    const btn = document.getElementById(btnId);
    if (!btn) return;

    if (loading) {
      btn.dataset.originalHtml = btn.innerHTML;
      btn.innerHTML = '<span class="spinner"></span>' + (loadingText || '处理中...');
      btn.disabled = true;
    } else {
      btn.innerHTML = btn.dataset.originalHtml || btn.innerHTML;
      btn.disabled = false;
    }
  }

  /**
   * HTML 转义防 XSS
   */
  function escapeHtml(str) {
    const div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
  }

  /**
   * 检查是否已登录（通过尝试访问受保护接口判断）
   * 此处简单实现：检查 URL 参数或直接跳转
   */
  function requireAuth() {
    // 后续可通过调用 API 验证 session 有效性
    // 当前仅做页面跳转逻辑
    return true;
  }

  return {
    showAlert,
    hideAlert,
    passwordStrength,
    updateStrengthUI,
    setLoading,
    requireAuth,
  };
})();
