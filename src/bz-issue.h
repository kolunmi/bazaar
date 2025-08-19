/* bz-issue.h
 *
 * Copyright 2025 Adam Masciola
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BZ_TYPE_ISSUE (bz_issue_get_type ())
G_DECLARE_FINAL_TYPE (BzIssue, bz_issue, BZ, ISSUE, GObject)

BzIssue *
bz_issue_new (void);

const char *
bz_issue_get_id (BzIssue *self);

const char *
bz_issue_get_url (BzIssue *self);

void
bz_issue_set_id (BzIssue    *self,
                 const char *id);

void
bz_issue_set_url (BzIssue    *self,
                  const char *url);

G_END_DECLS

/* End of bz-issue.h */
