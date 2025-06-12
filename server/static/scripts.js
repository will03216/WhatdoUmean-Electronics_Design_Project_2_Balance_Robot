// script.js
const statusEl = document.getElementById('status');
const btn = document.getElementById('goBtn');
// 目标重定向地址，按需修改：
const targetUrl = 'https://whatdoumeanrobot.com:60001/Web_UI.html/';

async function pollStatus() {
  try {
    const res = await fetch('/status');
    const { connected } = await res.json();
    if (!connected) {
      statusEl.textContent = 'Connected';
      statusEl.classList.remove('disconnected');
      statusEl.classList.add('connected');
      btn.disabled = false;
      btn.classList.add('enabled');
    } else {
      statusEl.textContent = 'Not Connected';
      statusEl.classList.remove('connected');
      statusEl.classList.add('disconnected');
      btn.disabled = true;
      btn.classList.remove('enabled');
    }
  } catch (err) {
    // 出错也当作未连接处理
    statusEl.textContent = 'Not Connected';
    statusEl.classList.remove('connected');
    statusEl.classList.add('disconnected');
    btn.disabled = true;
    btn.classList.remove('enabled');
    console.error('轮询错误:', err);
  }
}

// 初次执行一次，然后每秒轮询
pollStatus();
setInterval(pollStatus, 1000);

// 只有在 enabled（已连接）状态下才跳转
btn.addEventListener('click', () => {
  if (!btn.disabled) {
    window.location.href = targetUrl;
  }
});