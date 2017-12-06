/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UFDT_OVERLAY_INTERNAL_H
#define UFDT_OVERLAY_INTERNAL_H

#include <ufdt_types.h>

void *ufdt_get_fixup_location(struct ufdt *tree, const char *fixup);
int ufdt_do_one_fixup(struct ufdt *tree, const char *fixups, int fixups_len,
                      int phandle);
int ufdt_overlay_do_fixups(struct ufdt *main_tree, struct ufdt *overlay_tree);

#endif /* UFDT_OVERLAY_INTERNAL_H */
