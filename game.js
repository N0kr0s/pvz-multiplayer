const canvas = document.querySelector('#board');
const ctx = canvas.getContext('2d');
const labels = {
  peashooter: ['🌱 Горохострел', 100], sunflower: ['🌻 Подсолнух', 50], wallnut: ['🥜 Орех', 50],
  basic: ['🧟 Зомби', 25], cone: ['🚧 Конус', 50], bucket: ['🪣 Ведро', 80],
  grave: ['🪦 Могила', 65], farm: ['🏚️ Ферма мозгов', 90]
};
let token = null, role = null, selected = null, state = null;
const notice = document.querySelector('#notice');
const cellW = 100, cellH = 100;

async function api(path, body) {
  const response = await fetch(path, body ? {
    method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(body)
  } : undefined);
  const data = await response.json();
  if (!response.ok) throw Error(data.message || data.error || 'Ошибка сети');
  return data;
}

async function join(newRole) {
  try {
    const result = await api('/api/join', {role: newRole});
    token = result.token; role = newRole;
    sessionStorage.setItem('pvzmp-token', token);
    sessionStorage.setItem('pvzmp-role', role);
    startUi();
  } catch (error) { notice.textContent = error.message; alert(error.message); }
}

function startUi() {
  document.querySelector('#lobby').hidden = true;
  document.querySelector('#match').hidden = false;
  document.querySelector('#role').textContent = role === 'plants' ? 'Растения · хост' : 'Зомби · клиент';
  const kinds = role === 'plants' ? ['peashooter', 'sunflower', 'wallnut'] : ['basic', 'cone', 'bucket', 'grave', 'farm'];
  selected = kinds[0];
  const cards = document.querySelector('#cards');
  cards.replaceChildren(...kinds.map(kind => {
    const button = document.createElement('button');
    button.innerHTML = `${labels[kind][0]}<small>${labels[kind][1]} ${role === 'plants' ? '☀️' : '🧠'}</small>`;
    button.onclick = () => { selected = kind; updateCards(); };
    button.dataset.kind = kind;
    return button;
  }));
  updateCards();
  poll();
  setInterval(poll, 120);
}

function updateCards() {
  document.querySelectorAll('.cards button').forEach(button => button.classList.toggle('active', button.dataset.kind === selected));
}

async function poll() {
  if (!token) return;
  try {
    state = await api('/api/state');
    document.querySelector('#resources').textContent = `☀️ ${Math.floor(state.sun)}     🧠 ${Math.floor(state.brains)}`;
    document.querySelector('#timer').textContent = `${Math.floor(state.seconds)} c`;
    notice.textContent = state.phase === 'waiting' ? 'Ожидание второго игрока' :
      state.phase === 'finished' ? `Победа: ${state.winner === 'plants' ? 'растения' : 'зомби'} — ${state.message}` : state.message;
    draw();
  } catch (error) { notice.textContent = `Соединение потеряно: ${error.message}`; }
}

canvas.addEventListener('click', async event => {
  if (!token || !state || state.phase !== 'playing') return;
  const rect = canvas.getBoundingClientRect();
  const col = Math.floor((event.clientX - rect.left) * canvas.width / rect.width / cellW);
  const row = Math.floor((event.clientY - rect.top) * canvas.height / rect.height / cellH);
  try { await api('/api/action', {token, kind: selected, row, col}); await poll(); }
  catch (error) { notice.textContent = error.message; }
});

function emoji(symbol, col, row, size=46) {
  ctx.font = `${size}px system-ui`; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
  ctx.fillText(symbol, (col + .5) * cellW, (row + .5) * cellH);
}

function health(obj, max, x, y) {
  ctx.fillStyle = '#2b2119'; ctx.fillRect(x - 28, y + 25, 56, 7);
  ctx.fillStyle = obj.hp < max * .3 ? '#df5750' : '#89d54c';
  ctx.fillRect(x - 27, y + 26, 54 * Math.max(0, obj.hp / max), 5);
}

function draw() {
  if (!state) return;
  for (let row = 0; row < 5; row++) for (let col = 0; col < 9; col++) {
    ctx.fillStyle = (row + col) % 2 ? '#80ae4f' : '#8fbd58';
    ctx.fillRect(col * cellW, row * cellH, cellW, cellH);
    ctx.strokeStyle = '#5a8439'; ctx.strokeRect(col * cellW, row * cellH, cellW, cellH);
  }
  state.special.forEach(g => { emoji('🪦', g.col, g.row, 55); health(g, 260, (g.col+.5)*100, (g.row+.5)*100); });
  state.graves.forEach(g => { emoji('🪦', g.col, g.row); health(g, 180, (g.col+.5)*100, (g.row+.5)*100); });
  state.farms.forEach(g => { emoji('🏚️', g.col, g.row); health(g, 150, (g.col+.5)*100, (g.row+.5)*100); });
  state.plants.forEach(p => { emoji({peashooter:'🌱',sunflower:'🌻',wallnut:'🥜'}[p.kind],p.col,p.row); health(p, {peashooter:120,sunflower:100,wallnut:400}[p.kind],(p.col+.5)*100,(p.row+.5)*100); });
  state.zombies.forEach(z => { emoji({basic:'🧟',cone:'🚧',bucket:'🪣'}[z.kind],z.x,z.row); health(z,{basic:140,cone:300,bucket:540}[z.kind],(z.x+.5)*100,(z.row+.5)*100); });
}

document.querySelector('#plantRole').onclick = () => join('plants');
document.querySelector('#zombieRole').onclick = () => join('zombies');
document.querySelector('#address').textContent = `Адрес хоста: ${location.href}`;
const oldToken = sessionStorage.getItem('pvzmp-token');
const oldRole = sessionStorage.getItem('pvzmp-role');
if (oldToken && ['plants','zombies'].includes(oldRole)) { token = oldToken; role = oldRole; startUi(); }
