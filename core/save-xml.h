// SPDX-License-Identifier: GPL-2.0
#ifndef SAVE_XML_H
#define SAVE_XML_H
#include <vector>
struct divelog;
struct membuffer;
struct fingerprint_record;

// Serialize the current global dive log in Subsurface XML format.
int save_dives(const char *filename);
int save_dives_logic(const char *filename, bool select_only, bool anonymize);
// Serialize a staged log without swapping or inspecting the current model.
void serialize_divelog_xml(struct membuffer *buffer, struct divelog &log, const std::vector<fingerprint_record> &fingerprints);

#endif // SAVE_XML_H
