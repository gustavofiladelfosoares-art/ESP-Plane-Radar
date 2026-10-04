#include "ui/i18n.h"

#include <cmath>
#include <cstdio>

namespace ui::i18n {

Lang g_lang = Lang::PT;

namespace {

// Columns: Português, English, Español, 中文. Chinese glyphs are baked into
// the fonts by scripts/make_vlw.py from the characters used in this file.
constexpr const char* kText[static_cast<int>(S::Count)][kLangCount] = {
    {"Ensolarado", "Sunny", "Soleado", "晴"},
    {"Céu limpo", "Clear", "Despejado", "晴朗"},
    {"Poucas nuvens", "Few clouds", "Pocas nubes", "少云"},
    {"Parcialmente nublado", "Partly cloudy", "Parcialmente nublado", "多云"},
    {"Nublado", "Cloudy", "Nublado", "阴"},
    {"Neblina", "Fog", "Niebla", "雾"},
    {"Garoa", "Drizzle", "Llovizna", "毛毛雨"},
    {"Garoa gelada", "Freezing drizzle", "Llovizna helada", "冻毛毛雨"},
    {"Chuva fraca", "Light rain", "Lluvia ligera", "小雨"},
    {"Chuva", "Rain", "Lluvia", "中雨"},
    {"Chuva forte", "Heavy rain", "Lluvia fuerte", "大雨"},
    {"Chuva gelada", "Freezing rain", "Lluvia helada", "冻雨"},
    {"Neve", "Snow", "Nieve", "雪"},
    {"Pancadas de chuva", "Showers", "Chubascos", "阵雨"},
    {"Pancadas fortes", "Heavy showers", "Chubascos fuertes", "强阵雨"},
    {"Tempestade", "Thunderstorm", "Tormenta", "雷暴"},
    {"Tempestade c/ granizo", "Storm with hail", "Tormenta con granizo", "雷暴伴冰雹"},
    {"Tempo", "Weather", "Tiempo", "天气"},

    {"Sensação %d°", "Feels like %d°", "Sensación %d°", "体感 %d°"},
    {"Carregando clima…", "Loading weather…", "Cargando clima…", "正在加载天气…"},
    {"Sem Wi-Fi", "No Wi-Fi", "Sin Wi-Fi", "无 Wi-Fi"},

    {"Acertando…", "Syncing…", "Sincronizando…", "正在校时…"},

    {"MAIS PRÓXIMO", "NEAREST", "MÁS CERCANO", "最近的飞机"},
    {"Aeronave", "Aircraft", "Aeronave", "飞机"},
    {"No solo", "On ground", "En tierra", "在地面"},
    {"raio %s", "radius %s", "radio %s", "半径 %s"},
    {"Raio: %s", "Radius: %s", "Radio: %s", "半径: %s"},
    {"Nenhum avião", "No aircraft", "Ningún avión", "附近没有飞机"},
    {"em até %s", "within %s", "hasta %s", "%s 范围内"},
    {"2 toques: mudar raio", "2 taps: change radius", "2 toques: cambiar radio", "双击：更改半径"},
    {"tentando conectar…", "trying to connect…", "intentando conectar…", "正在尝试连接…"},

    {"Boa", "Good", "Buena", "优"},
    {"Moderada", "Moderate", "Moderada", "良"},
    {"Ruim p/ sens.", "Sensitive", "Dañina (sens.)", "轻度污染"},
    {"Ruim", "Unhealthy", "Dañina", "中度污染"},
    {"Muito ruim", "Very unhealthy", "Muy dañina", "重度污染"},
    {"Péssima", "Hazardous", "Peligrosa", "严重污染"},
    {"Baixo", "Low", "Bajo", "低"},
    {"Moderado", "Moderate", "Moderado", "中等"},
    {"Alto", "High", "Alto", "高"},
    {"Muito alto", "Very high", "Muy alto", "很高"},
    {"Extremo", "Extreme", "Extremo", "极高"},
    {"AR", "AIR", "AIRE", "空气"},
    {"até o pôr do sol", "until sunset", "hasta el ocaso", "距日落"},
    {"até o sol nascer", "until sunrise", "hasta el amanecer", "距日出"},
    {"Sol: aguardando…", "Sun: waiting…", "Sol: esperando…", "日照：等待中…"},
    {"Carregando ar e UV…", "Loading air & UV…", "Cargando aire y UV…", "正在加载空气…"},

    {"Configurar Wi-Fi", "Wi-Fi setup", "Configurar Wi-Fi", "设置 Wi-Fi"},
    {"1. No celular, entre na rede:", "1. On your phone, join:", "1. En el celular, únete a:", "1. 用手机连接网络："},
    {"2. Abra no navegador:", "2. Open in the browser:", "2. Abre en el navegador:", "2. 在浏览器中打开："},
    {"ou 192.168.4.1", "or 192.168.4.1", "o 192.168.4.1", "或 192.168.4.1"},
    {"Conectando a", "Connecting to", "Conectando a", "正在连接"},
    {"Não conectou", "Not connected", "No se conectó", "连接失败"},
    {"Confira a senha", "Check the password", "Revisa la contraseña", "请检查密码"},
    {"e o sinal do Wi-Fi.", "and the Wi-Fi signal.", "y la señal Wi-Fi.", "和 Wi-Fi 信号"},
    {"Segure BOOT por 3 s", "Hold BOOT for 3 s", "Mantén BOOT 3 s", "按住 BOOT 3 秒"},
    {"para configurar de novo", "to set up again", "para configurar de nuevo", "以重新设置"},
    {"Wi-Fi apagado", "Wi-Fi cleared", "Wi-Fi borrado", "Wi-Fi 已清除"},
    {"Reiniciando…", "Restarting…", "Reiniciando…", "正在重启…"},

    {"SOBRE VOCÊ!", "OVERHEAD!", "¡SOBRE TI!", "头顶飞过！"},
    {"EMERGÊNCIA", "EMERGENCY", "EMERGENCIA", "紧急情况"},
    {"FALHA DE RÁDIO", "RADIO FAILURE", "FALLA DE RADIO", "无线电故障"},
    {"SEQUESTRO", "HIJACK", "SECUESTRO", "劫机"},
    {"MILITAR", "MILITARY", "MILITAR", "军用"},

    {"Hoje", "Today", "Hoy", "今天"},
    {"Ano Novo", "New Year", "Año Nuevo", "元旦"},
    {"Carnaval", "Carnival", "Carnaval", "狂欢节"},
    {"Sexta Santa", "Good Friday", "Viernes Santo", "耶稣受难日"},
    {"Tiradentes", "Tiradentes", "Tiradentes", "蒂拉登特斯日"},
    {"Dia do Trabalho", "Labour Day", "Día del Trabajo", "劳动节"},
    {"Corpus Christi", "Corpus Christi", "Corpus Christi", "基督圣体节"},
    {"Independência", "Independence Day", "Independencia", "独立日"},
    {"Aparecida", "Aparecida", "Aparecida", "圣母显灵节"},
    {"Finados", "All Souls' Day", "Día de Difuntos", "万灵节"},
    {"República", "Republic Day", "Día de la República", "共和国日"},
    {"Consciência Negra", "Black Awareness", "Conciencia Negra", "黑人意识日"},
    {"Natal", "Christmas", "Navidad", "圣诞节"},

    {"PRÓXIMOS DIAS", "NEXT DAYS", "PRÓXIMOS DÍAS", "未来五天"},
    {"PRÓXIMAS HORAS", "NEXT HOURS", "PRÓXIMAS HORAS", "未来12小时"},
    {"HOJE NO CÉU", "TODAY IN THE SKY", "HOY EN EL CIELO", "今日天空"},
    {"aviões vistos", "aircraft seen", "aviones vistos", "架飞机"},
    {"Mais alto", "Highest", "Más alto", "最高"},
    {"Mais rápido", "Fastest", "Más rápido", "最快"},
    {"Mais perto", "Closest", "Más cerca", "最近"},
    {"Mais vista", "Most seen", "Más vista", "最常见"},
    {"%d voos", "%d flights", "%d vuelos", "%d 次"},
    {"Contando os aviões…", "Counting aircraft…", "Contando aviones…", "正在统计飞机…"},
    {"AGENDA", "AGENDA", "AGENDA", "日程"},
    {"Dia livre!", "Free day!", "¡Día libre!", "今天很轻松！"},
    {"Nada marcado hoje", "Nothing planned today", "Nada para hoy", "今天没有安排"},
    {"Carregando agenda…", "Loading agenda…", "Cargando agenda…", "正在加载日程…"},
    {"Cole o link da agenda em", "Paste your agenda link at", "Pega el enlace de la agenda en",
     "在此粘贴日程链接："},
    {"dia todo", "all day", "todo el día", "全天"},
    {"%d de %d", "%d of %d", "%d de %d", "%d / %d"},
};

constexpr const char* kWeekInitials[kLangCount][7] = {
    {"D", "S", "T", "Q", "Q", "S", "S"},
    {"S", "M", "T", "W", "T", "F", "S"},
    {"D", "L", "M", "X", "J", "V", "S"},
    {"日", "一", "二", "三", "四", "五", "六"},
};

constexpr const char* kMonthsFull[kLangCount][12] = {
    {"JANEIRO", "FEVEREIRO", "MARÇO", "ABRIL", "MAIO", "JUNHO", "JULHO", "AGOSTO", "SETEMBRO",
     "OUTUBRO", "NOVEMBRO", "DEZEMBRO"},
    {"JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE", "JULY", "AUGUST", "SEPTEMBER",
     "OCTOBER", "NOVEMBER", "DECEMBER"},
    {"ENERO", "FEBRERO", "MARZO", "ABRIL", "MAYO", "JUNIO", "JULIO", "AGOSTO", "SEPTIEMBRE",
     "OCTUBRE", "NOVIEMBRE", "DICIEMBRE"},
    {"1月", "2月", "3月", "4月", "5月", "6月", "7月", "8月", "9月", "10月", "11月", "12月"},
};

constexpr const char* kWeekdays[kLangCount][7] = {
    {"DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SÁB"},
    {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"},
    {"DOM", "LUN", "MAR", "MIÉ", "JUE", "VIE", "SÁB"},
    {"周日", "周一", "周二", "周三", "周四", "周五", "周六"},
};

constexpr const char* kMonths[kLangCount][12] = {
    {"JAN", "FEV", "MAR", "ABR", "MAI", "JUN", "JUL", "AGO", "SET", "OUT", "NOV", "DEZ"},
    {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"},
    {"ENE", "FEB", "MAR", "ABR", "MAY", "JUN", "JUL", "AGO", "SEP", "OCT", "NOV", "DIC"},
    {"1月", "2月", "3月", "4月", "5月", "6月", "7月", "8月", "9月", "10月", "11月", "12月"},
};

constexpr const char* kCompass[kLangCount][8] = {
    {"N", "NE", "L", "SE", "S", "SO", "O", "NO"},
    {"N", "NE", "E", "SE", "S", "SW", "W", "NW"},
    {"N", "NE", "E", "SE", "S", "SO", "O", "NO"},
    {"北", "东北", "东", "东南", "南", "西南", "西", "西北"},
};

// Radar letters use the ASCII-only radar font, so Chinese keeps N/S/E/W.
constexpr const char* kCardinals[kLangCount][4] = {
    {"N", "S", "L", "O"},
    {"N", "S", "E", "W"},
    {"N", "S", "E", "O"},
    {"N", "S", "E", "W"},
};

int lang() {
  const int l = static_cast<int>(g_lang);
  return l >= 0 && l < kLangCount ? l : 0;
}

}  // namespace

const char* tr(S s) { return kText[static_cast<int>(s)][lang()]; }

const char* weekday(int wday) { return kWeekdays[lang()][((wday % 7) + 7) % 7]; }

const char* weekdayInitial(int wday) { return kWeekInitials[lang()][((wday % 7) + 7) % 7]; }

void formatMonthYear(char* out, size_t len, int m, int year) {
  const char* name = kMonthsFull[lang()][((m - 1) % 12 + 12) % 12];
  if (g_lang == Lang::ZH) {
    snprintf(out, len, "%d年%s", year, name);
  } else {
    snprintf(out, len, "%s %d", name, year);
  }
}

const char* month(int m) { return kMonths[lang()][((m - 1) % 12 + 12) % 12]; }

void formatDate(char* out, size_t len, int wday, int day, int m) {
  if (g_lang == Lang::ZH) {
    snprintf(out, len, "%s%d日 %s", month(m), day, weekday(wday));
  } else {
    snprintf(out, len, "%s, %d %s", weekday(wday), day, month(m));
  }
}

const char* compass8(float deg) {
  const int i = static_cast<int>(lroundf(fmodf(deg + 360.0f, 360.0f) / 45.0f)) % 8;
  return kCompass[lang()][i];
}

void formatDistanceDir(char* out, size_t len, float km, const char* dir) {
  char num[16];
  if (km < 10.0f) {
    const int tenths = static_cast<int>(lroundf(km * 10.0f));
    snprintf(num, sizeof(num), "%d%c%d km", tenths / 10, decimalSep(), tenths % 10);
  } else {
    snprintf(num, sizeof(num), "%d km", static_cast<int>(lroundf(km)));
  }
  switch (g_lang) {
    case Lang::PT:
      snprintf(out, len, "%s a %s", num, dir);
      break;
    case Lang::ES:
      snprintf(out, len, "%s al %s", num, dir);
      break;
    default:
      snprintf(out, len, "%s %s", num, dir);
      break;
  }
}

char decimalSep() { return g_lang == Lang::PT || g_lang == Lang::ES ? ',' : '.'; }

char thousandsSep() { return g_lang == Lang::PT || g_lang == Lang::ES ? '.' : ','; }

const char* cardinal(int idx) { return kCardinals[lang()][idx & 3]; }

}  // namespace ui::i18n
