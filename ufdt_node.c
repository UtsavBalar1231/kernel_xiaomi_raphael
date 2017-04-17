/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "libufdt.h"

bool ufdt_node_name_eq(const struct ufdt_node *node, const char *name, int len) {
  if (!node) return false;
  if (!name) return false;
  if (dto_strncmp(ufdt_node_name(node), name, len) != 0) return false;
  if (ufdt_node_name(node)[len] != '\0') return false;
  return true;
}

/*
 * ufdt_node methods.
 */

struct ufdt_node *ufdt_node_construct(void *fdtp, fdt32_t *fdt_tag_ptr) {
  uint32_t tag = fdt32_to_cpu(*fdt_tag_ptr);
  if (tag == FDT_PROP) {
    const struct fdt_property *prop = (const struct fdt_property *)fdt_tag_ptr;
    struct ufdt_node_fdt_prop *res =
        dto_malloc(sizeof(struct ufdt_node_fdt_prop));
    if (res == NULL) return NULL;
    res->parent.fdt_tag_ptr = fdt_tag_ptr;
    res->parent.sibling = NULL;
    res->name = fdt_string(fdtp, fdt32_to_cpu(prop->nameoff));
    return (struct ufdt_node *)res;
  } else {
    struct ufdt_node_fdt_node *res =
        dto_malloc(sizeof(struct ufdt_node_fdt_node));
    if (res == NULL) return NULL;
    res->parent.fdt_tag_ptr = fdt_tag_ptr;
    res->parent.sibling = NULL;
    res->child = NULL;
    res->last_child_p = &res->child;
    return (struct ufdt_node *)res;
  }
}

void ufdt_node_destruct(struct ufdt_node *node) {
  if (node == NULL) return;

  if (ufdt_node_tag(node) == FDT_BEGIN_NODE) {
    ufdt_node_destruct(((struct ufdt_node_fdt_node *)node)->child);
  }

  ufdt_node_destruct(node->sibling);
  dto_free(node);
}

int ufdt_node_add_child(struct ufdt_node *parent, struct ufdt_node *child) {
  if (!parent || !child) return -1;
  if (ufdt_node_tag(parent) != FDT_BEGIN_NODE) return -1;

  int err = 0;
  uint32_t child_tag = ufdt_node_tag(child);
  switch (child_tag) {
    case FDT_PROP:
    case FDT_BEGIN_NODE:
      // Append the child node to the last child of parant node
      *((struct ufdt_node_fdt_node *)parent)->last_child_p = child;
      ((struct ufdt_node_fdt_node *)parent)->last_child_p = &child->sibling;
      break;

    default:
      err = -1;
      dto_error("invalid children tag type\n");
  }

  return err;
}

/*
 * BEGIN of FDT_PROP related methods.
 */

struct ufdt_node *ufdt_node_get_subnode_by_name_len(const struct ufdt_node *node,
                                                  const char *name, int len) {
  struct ufdt_node **it = NULL;
  for_each_node(it, node) {
    if (ufdt_node_name_eq(*it, name, len)) return *it;
  }
  return NULL;
}

struct ufdt_node *ufdt_node_get_subnode_by_name(const struct ufdt_node *node,
                                              const char *name) {
  return ufdt_node_get_subnode_by_name_len(node, name, strlen(name));
}

struct ufdt_node *ufdt_node_get_property_by_name_len(
    const struct ufdt_node *node, const char *name, int len) {
  if (!node) return NULL;

  struct ufdt_node **it = NULL;
  for_each_prop(it, node) {
    if (ufdt_node_name_eq(*it, name, len)) return *it;
  }
  return NULL;
}

struct ufdt_node *ufdt_node_get_property_by_name(const struct ufdt_node *node,
                                                 const char *name) {
  return ufdt_node_get_property_by_name_len(node, name, dto_strlen(name));
}

char *ufdt_node_get_fdt_prop_data(const struct ufdt_node *node, int *out_len) {
  if (!node || ufdt_node_tag(node) != FDT_PROP) {
    return NULL;
  }
  const struct fdt_property *prop = (struct fdt_property *)node->fdt_tag_ptr;
  if (out_len != NULL) {
    *out_len = fdt32_to_cpu(prop->len);
  }
  return (char *)prop->data;
}

char *ufdt_node_get_fdt_prop_data_by_name_len(const struct ufdt_node *node,
                                              const char *name, int len,
                                              int *out_len) {
  return ufdt_node_get_fdt_prop_data(
      ufdt_node_get_property_by_name_len(node, name, len), out_len);
}

char *ufdt_node_get_fdt_prop_data_by_name(const struct ufdt_node *node,
                                          const char *name, int *out_len) {
  return ufdt_node_get_fdt_prop_data(ufdt_node_get_property_by_name(node, name),
                                     out_len);
}

/*
 * END of FDT_PROP related methods.
 */

/*
 * BEGIN of searching-in-ufdt_node methods.
 */

uint32_t ufdt_node_get_phandle(const struct ufdt_node *node) {
  if (!node || ufdt_node_tag(node) != FDT_BEGIN_NODE) {
    return 0;
  }
  int len = 0;
  void *ptr = ufdt_node_get_fdt_prop_data_by_name(node, "phandle", &len);
  if (!ptr || len != sizeof(fdt32_t)) {
    ptr = ufdt_node_get_fdt_prop_data_by_name(node, "linux,phandle", &len);
    if (!ptr || len != sizeof(fdt32_t)) {
      return 0;
    }
  }
  return fdt32_to_cpu(*((fdt32_t *)ptr));
}

struct ufdt_node *ufdt_node_get_node_by_path_len(const struct ufdt_node *node,
                                                 const char *path, int len) {
  const char *end = path + len;

  struct ufdt_node *cur = (struct ufdt_node *)node;

  while (path < end) {
    while (path[0] == '/') path++;
    if (path == end) return cur;

    const char *next_slash;
    next_slash = dto_memchr(path, '/', end - path);
    if (!next_slash) next_slash = end;

    struct ufdt_node *next = NULL;

    next = ufdt_node_get_subnode_by_name_len(cur, path, next_slash - path);

    cur = next;
    path = next_slash;
    if (!cur) return cur;
  }

  return cur;
}

struct ufdt_node *ufdt_node_get_node_by_path(const struct ufdt_node *node,
                                             const char *path) {
  return ufdt_node_get_node_by_path_len(node, path, dto_strlen(path));
}

/*
 * END of searching-in-ufdt_node methods.
 */

#define TAB_SIZE 2

void ufdt_node_print(const struct ufdt_node *node, int depth) {
  if (!node) return;

  int i;
  for (i = 0; i < depth * TAB_SIZE; i++) dto_print(" ");

  uint32_t tag;
  tag = ufdt_node_tag(node);

  switch (tag) {
    case FDT_BEGIN_NODE:
      dto_print("NODE ");
      break;
    case FDT_PROP:
      dto_print("PROP ");
      break;
    default:
      dto_print("UNKNOWN ");
      break;
  }

  if (ufdt_node_name(node)) {
    dto_print(":%s:\n", ufdt_node_name(node));
  } else {
    dto_print("node name is NULL.\n");
  }

  if (ufdt_node_tag(node) == FDT_BEGIN_NODE) {
    struct ufdt_node **it;

    for_each_prop(it, node) ufdt_node_print(*it, depth + 1);

    for_each_node(it, node) ufdt_node_print(*it, depth + 1);
  }
}
