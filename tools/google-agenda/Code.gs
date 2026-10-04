/**
 * ESP Plane Radar — ponte com a Google Agenda (Google Apps Script).
 *
 * Devolve só o que é de HOJE, num JSON pequeno que o aparelho consegue ler:
 *   {"events":[{"time":"09:30","title":"Dentista"}],
 *    "tasks":[{"title":"Comprar pão","done":false}]}
 *
 * Como instalar: veja o README.md desta pasta.
 * Este link dá acesso aos títulos da sua agenda de hoje — não compartilhe.
 */

// Quantos itens mandar no máximo (o aparelho mostra até 12).
var MAX_EVENTS = 8;
var MAX_TASKS = 6;

function doGet() {
  var tz = Session.getScriptTimeZone();
  var now = new Date();
  var today = Utilities.formatDate(now, tz, 'yyyy-MM-dd');
  var start = parseLocal_(today, tz);
  var end = new Date(start.getTime() + 24 * 3600 * 1000);
  var out = { events: todayEvents_(start, end, tz), tasks: todayTasks_(today, tz) };
  return ContentService.createTextOutput(JSON.stringify(out))
      .setMimeType(ContentService.MimeType.JSON);
}

/** Eventos de hoje de todas as agendas visíveis (sem a de feriados). */
function todayEvents_(start, end, tz) {
  var list = [];
  CalendarApp.getAllCalendars().forEach(function (cal) {
    if (cal.isHidden() || !cal.isSelected() || cal.getId().indexOf('#holiday@') >= 0) return;
    cal.getEvents(start, end).forEach(function (ev) {
      var allDay = ev.isAllDayEvent();
      list.push({
        sort: allDay ? 0 : ev.getStartTime().getTime(),
        time: allDay ? '' : Utilities.formatDate(ev.getStartTime(), tz, 'HH:mm'),
        title: clean_(ev.getTitle())
      });
    });
  });
  list.sort(function (a, b) { return a.sort - b.sort; });
  return list.slice(0, MAX_EVENTS).map(function (e) { return { time: e.time, title: e.title }; });
}

/**
 * Tarefas (Google Tasks) para hoje: pendentes com prazo até hoje (ou sem
 * prazo) e as concluídas hoje. Precisa do serviço "Tasks API" ativado; sem
 * ele, a agenda funciona só com os eventos.
 */
function todayTasks_(today, tz) {
  if (typeof Tasks === 'undefined') return [];
  var list = [];
  try {
    (Tasks.Tasklists.list().items || []).forEach(function (tl) {
      var res = Tasks.Tasks.list(tl.id, { showCompleted: true, showHidden: true, maxResults: 100 });
      (res.items || []).forEach(function (t) {
        if (!t.title) return;
        var due = t.due ? t.due.substring(0, 10) : '';
        var done = t.status === 'completed';
        var doneToday = done && t.completed &&
            Utilities.formatDate(new Date(t.completed), tz, 'yyyy-MM-dd') === today;
        if ((!done && (due === '' || due <= today)) || doneToday) {
          list.push({ title: clean_(t.title), done: done, due: due });
        }
      });
    });
  } catch (e) {
    return [];
  }
  // Pendentes primeiro (com prazo antes das sem prazo), concluídas no fim.
  list.sort(function (a, b) {
    if (a.done !== b.done) return a.done ? 1 : -1;
    return (a.due || '9') < (b.due || '9') ? -1 : 1;
  });
  return list.slice(0, MAX_TASKS).map(function (t) { return { title: t.title, done: t.done }; });
}

/** Meia-noite de hoje no fuso da agenda. */
function parseLocal_(ymd, tz) {
  var offset = Utilities.formatDate(new Date(), tz, 'XXX');  // ex.: -03:00
  return new Date(ymd + 'T00:00:00' + offset);
}

/** Tira emojis e símbolos que a tela não tem, e limita o tamanho. */
function clean_(s) {
  s = String(s || '').replace(/[^\u0000-ɏ‐-‧　-〿一-鿿＀-￯]/g, '');
  s = s.replace(/\s+/g, ' ').trim();
  return s.length > 40 ? s.substring(0, 39) + '…' : s;
}
