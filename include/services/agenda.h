#pragma once

#include "ui/model.h"

namespace services::agenda {

/**
 * Fetch today's agenda from the user's Google Apps Script link (see
 * tools/google-agenda). Does nothing when no link is configured.
 */
bool fetch(const char* url);

/** Copy of the latest agenda for drawing. */
void snapshot(ui::AgendaModel* out);

}  // namespace services::agenda
