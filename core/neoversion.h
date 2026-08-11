// SPDX-License-Identifier: GPL-2.0
#ifndef SUBSURFACE_NEO_VERSION_H
#define SUBSURFACE_NEO_VERSION_H

// Subsurface Neo has its own release cadence. Keep this independent from the
// upstream Subsurface build number so upstream synchronization cannot by itself
// advertise a new Neo release.
#define SUBSURFACE_NEO_VERSION "0.1.0-dev"

inline const char *subsurface_neo_version()
{
	return SUBSURFACE_NEO_VERSION;
}

#endif // SUBSURFACE_NEO_VERSION_H
