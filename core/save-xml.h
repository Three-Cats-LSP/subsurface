// SPDX-License-Identifier: GPL-2.0
#ifndef SAVE_XML_H
#define SAVE_XML_H

// Serialize the current global dive log in Subsurface XML format.
int save_dives(const char *filename);
int save_dives_logic(const char *filename, bool select_only, bool anonymize);

#endif // SAVE_XML_H
