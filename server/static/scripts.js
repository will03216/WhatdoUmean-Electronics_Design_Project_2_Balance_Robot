// script.js
const statusEl = document.getElementById('status');
const btn = document.getElementById('goBtn');
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
    statusEl.textContent = 'Not Connected';
    statusEl.classList.remove('connected');
    statusEl.classList.add('disconnected');
    btn.disabled = true;
    btn.classList.remove('enabled');
    console.error('轮询错误:', err);
  }
}

pollStatus();
setInterval(pollStatus, 1000);

btn.addEventListener('click', () => {
  if (!btn.disabled) {
    window.location.href = targetUrl;
  }
});