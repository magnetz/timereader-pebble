#include <pebble.h>
#include <string.h>
#include "strings.h"

static const char *EN[STR__COUNT] = {
  [STR_NO_BOOKS]       = "No books\nadd from your phone",
  [STR_START_PAGE]     = "Start page",
  [STR_END_PAGE]       = "End page",
  [STR_END_LT_START]   = "End page < start page",
  [STR_PAGES_UNKNOWN]  = "Total pages\nunknown",
  [STR_PAUSE]          = "PAUSED",
  [STR_PAGE_FMT]       = "Page %d",
  [STR_PAGES_FMT]      = "%d pages",
  [STR_PPH_FMT]        = "%d.%d pg/h%s",
  [STR_EST]            = " (est.)",
  [STR_TIME_TOTAL_FMT] = "Total %d.%02d h",
  [STR_LEFT_H_FMT]     = "%dp left ~%d h%s",
  [STR_LEFT_MIN_FMT]   = "%dp left ~%d min%s",
  [STR_MENU_SAVE]      = "Save end page",
  [STR_MENU_EXIT]      = "Exit without saving",
  [STR_MENU_CANCEL]    = "Cancel",
  [STR_EXIT_CONFIRM]   = "Exit without saving?",
  [STR_EXIT_HINT]      = "Select: exit\nBack: cancel",
  [STR_SESSION_SAVED]  = "Session saved",
  [STR_SUMMARY_HINT]   = "Up or long Back = undo",
};

static const char *IT[STR__COUNT] = {
  [STR_NO_BOOKS]       = "Nessun libro\naggiungi dal telefono",
  [STR_START_PAGE]     = "Pagina iniziale",
  [STR_END_PAGE]       = "Pagina finale",
  [STR_END_LT_START]   = "Pagina finale < iniziale",
  [STR_PAGES_UNKNOWN]  = "Pagine totali\nsconosciute",
  [STR_PAUSE]          = "PAUSA",
  [STR_PAGE_FMT]       = "Pag. %d",
  [STR_PAGES_FMT]      = "%d pag.",
  [STR_PPH_FMT]        = "%d.%d pag/ora%s",
  [STR_EST]            = " (stima)",
  [STR_TIME_TOTAL_FMT] = "Tempo tot %d.%02d h",
  [STR_LEFT_H_FMT]     = "Resta %dp ~%d h%s",
  [STR_LEFT_MIN_FMT]   = "Resta %dp ~%d min%s",
  [STR_MENU_SAVE]      = "Salva pagina finale",
  [STR_MENU_EXIT]      = "Esci senza salvare",
  [STR_MENU_CANCEL]    = "Annulla",
  [STR_EXIT_CONFIRM]   = "Uscire senza salvare?",
  [STR_EXIT_HINT]      = "Select: esci\nIndietro: annulla",
  [STR_SESSION_SAVED]  = "Sessione salvata",
  [STR_SUMMARY_HINT]   = "Su o Indietro-lungo = annulla",
};

static const char **table(void) {
  static const char **cached;
  if (!cached) {
    const char *loc = i18n_get_system_locale();
    cached = (loc && loc[0] == 'i' && loc[1] == 't') ? IT : EN;
  }
  return cached;
}

const char *S(StringId id) {
  if (id >= STR__COUNT) return "";
  const char *s = table()[id];
  return s ? s : (EN[id] ? EN[id] : "");
}
