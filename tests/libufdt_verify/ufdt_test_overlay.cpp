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

#include <string>

extern "C" {

#include "libufdt.h"
#include "ufdt_node_pool.h"
#include "ufdt_overlay.h"
#include "ufdt_overlay_internal.h"

}

#include "ufdt_test_overlay.h"

static bool ufdt_node_compare(struct ufdt_node *node_a, struct ufdt_node *node_b,
                              struct ufdt* tree_a, struct ufdt* tree_b);

/*
 * Helper method to check if the tree rooted at node_b is a subset of the tree rooted
 * at node_a.
 */
static bool compare_child_nodes(struct ufdt_node *node_a, struct ufdt_node *node_b,
                                struct ufdt * tree_a, struct ufdt * tree_b) {
    bool result = true;
    struct ufdt_node *it;

    for (it = ((struct ufdt_node_fdt_node *)node_b)->child; it; it = it->sibling) {
        struct ufdt_node *cur_node = it;
        struct ufdt_node *target_node = NULL;

        if (ufdt_node_tag(cur_node) == FDT_BEGIN_NODE) {
            target_node =
                    ufdt_node_get_subnode_by_name(node_a, ufdt_node_name(cur_node));
        } else {
            target_node =
                    ufdt_node_get_property_by_name(node_a, ufdt_node_name(cur_node));
        }

        if (target_node == NULL) {
            result = false;
        } else {
            result = ufdt_node_compare(target_node, cur_node, tree_a, tree_b);
        }

        if (!result) {
            break;
        }
    }

    return result;
}

/*
 * Method to compare two nodes with tag FDT_PROP. Also accounts for the cases where
 * the property type is phandle.
 */
static bool ufdt_compare_property(struct ufdt_node* node_final, struct ufdt_node* node_overlay,
                                  struct ufdt* tree_final, struct ufdt* tree_overlay) {
    if (ufdt_node_tag(node_final) == FDT_PROP) {
        /* Return -1 if property names are differemt */
        if (strcmp(ufdt_node_name(node_final), ufdt_node_name(node_overlay)) != 0)
            return false;

        int length_data_final = 0, length_data_overlay = 0;
        char *prop_data_final = ufdt_node_get_fdt_prop_data(node_final, &length_data_final);
        char *prop_data_overlay = ufdt_node_get_fdt_prop_data(node_overlay,
                                                              &length_data_overlay);

        /* Confirm length for the property values are the same */
        if (length_data_final != length_data_overlay) {
            return false;
        }

        if (((length_data_final == 0) && (length_data_overlay ==0)) ||
            (memcmp(prop_data_final, prop_data_overlay, length_data_final) == 0)) {
            // Return if the properties have same value.
            return true;
        } else {
            /* check for the presence of phandles */
            for (int i = 0; i < length_data_final; i += sizeof(fdt32_t)) {
                int offset_data_a = fdt32_to_cpu(
                        *reinterpret_cast<fdt32_t *>(prop_data_final + i));
                int offset_data_b = fdt32_to_cpu(
                        *reinterpret_cast<fdt32_t *>(prop_data_overlay + i));
                if (offset_data_a == offset_data_b) continue;
                /* If the offsets have phandles, they would have valid target nodes */
                struct ufdt_node * target_node_a = ufdt_get_node_by_phandle(tree_final,
                                                                            offset_data_a);
                struct ufdt_node * target_node_b = ufdt_get_node_by_phandle(tree_overlay,
                                                                            offset_data_b);

                /*
                 * verify that the target nodes are valid and point to the same node.
                 */
                if ((target_node_a == NULL) || (target_node_b == NULL) ||
                    strcmp(ufdt_node_name(target_node_a),
                           ufdt_node_name(target_node_b)) != 0) {
                    return false;
                }
            }
        }
    }

    return true;
}

/*
 * Checks if the ufdt tree rooted at node_b is a subtree of the tree rooted at
 * node_a.
 */
static bool ufdt_node_compare(struct ufdt_node *node_final, struct ufdt_node *node_overlay,
                              struct ufdt * tree_final, struct ufdt * tree_overlay) {
    if (ufdt_node_tag(node_final) == FDT_PROP) {
        return ufdt_compare_property(node_final, node_overlay, tree_final, tree_overlay);
    }

    return compare_child_nodes(node_final, node_overlay, tree_final, tree_overlay);
}


/*
 * Multiple fragments may fixup to the same node on the base device tree.
 * Combine these fragments for easier verification.
 */
void ufdt_combine_fixup(struct ufdt *tree, const char *fixup,
                        struct ufdt_node **prev_node, struct ufdt_node_pool *node_pool) {
    char *path, *prop_ptr, *offset_ptr;
    char path_buf[1024];
    char *path_mem = NULL;
    int result = 0;

    size_t fixup_len = strlen(fixup) + 1;
    if (fixup_len > sizeof(path_buf)) {
        path_mem = static_cast<char *>(dto_malloc(fixup_len));
        path = path_mem;
    } else {
        path = path_buf;
    }
    dto_memcpy(path, fixup, fixup_len);

    prop_ptr = dto_strchr(path, ':');
    if (prop_ptr == NULL) {
        dto_error("Missing property part in '%s'\n", path);
        goto fail;
    }

    *prop_ptr = '\0';
    prop_ptr++;

    offset_ptr = dto_strchr(prop_ptr, ':');
    if (offset_ptr == NULL) {
        dto_error("Missing offset part in '%s'\n", path);
        goto fail;
    }

    *offset_ptr = '\0';
    offset_ptr++;

    result = dto_strcmp(prop_ptr, "target");
    /* If the property being fixed up is not target, ignore and return */
    if (result == 0) {
        struct ufdt_node *target_node;
        target_node = ufdt_get_node_by_path(tree, path);
        if (target_node == NULL) {
            dto_error("Path '%s' not found\n", path);
        } else {
            /* The goal is to combine fragments that have a common target */
            if (*prev_node != NULL) {
                ufdt_node_merge_into(*prev_node, target_node, node_pool);
            } else {
                *prev_node = target_node;
            }
        }
    }

fail:
    if (path_mem) {
        dto_free(path_mem);
    }

    return;
}

/*
 * Method to combine fragments fixing up to the same target node.
 */
static void ufdt_combine_one_fixup(struct ufdt *tree, const char *fixups,
                                  int fixups_len, struct ufdt_node_pool *pool) {
    struct ufdt_node* prev_node = NULL;

    while (fixups_len > 0) {
        ufdt_combine_fixup(tree, fixups, &prev_node, pool);
        fixups_len -= dto_strlen(fixups) + 1;
        fixups += dto_strlen(fixups) + 1;
    }

    return;
}

/*
 * Handle __fixups__ node in overlay tree. Majority of the code reused from
 * ufdt_overlay.c
 */
static int ufdt_overlay_do_fixups_and_combine(struct ufdt *final_tree,
                                              struct ufdt *overlay_tree,
                                              struct ufdt_node_pool *pool) {
    if (ufdt_overlay_do_fixups(final_tree, overlay_tree)) {
        return -1;
    }

    int len = 0;
    struct ufdt_node *overlay_fixups_node =
            ufdt_get_node_by_path(overlay_tree, "/__fixups__");

    /*
     * Combine all fragments with the same fixup.
     */

    struct ufdt_node** it;
    for_each_prop(it, overlay_fixups_node) {
        struct ufdt_node *fixups = *it;
        const char *fixups_paths = ufdt_node_get_fdt_prop_data(fixups, &len);
        ufdt_combine_one_fixup(overlay_tree, fixups_paths, len, pool);
    }

    return 0;
}
/* END of doing fixup in the overlay ufdt. */

static bool ufdt_verify_overlay_node(struct ufdt_node *target_node,
                                     struct ufdt_node *overlay_node,
                                     struct ufdt * target_tree,
                                     struct ufdt * overlay_tree) {
    return ufdt_node_compare(target_node, overlay_node, target_tree, overlay_tree);
}

enum overlay_result {
    OVERLAY_RESULT_OK,
    OVERLAY_RESULT_MISSING_TARGET,
    OVERLAY_RESULT_MISSING_OVERLAY,
    OVERLAY_RESULT_TARGET_PATH_INVALID,
    OVERLAY_RESULT_TARGET_INVALID,
    OVERLAY_RESULT_VERIFY_FAIL,
};

/*
 * verify one overlay fragment (subtree).
 */
static int ufdt_verify_fragment(struct ufdt *tree,
                                struct ufdt *overlay_tree,
                                struct ufdt_node *frag_node) {
    uint32_t target;
    const char *target_path;
    const void *val;
    struct ufdt_node *target_node = NULL;
    struct ufdt_node *overlay_node = NULL;

    val = ufdt_node_get_fdt_prop_data_by_name(frag_node, "target", NULL);
    if (val) {
        dto_memcpy(&target, val, sizeof(target));
        target = fdt32_to_cpu(target);
        target_node = ufdt_get_node_by_phandle(tree, target);
        if (target_node == NULL) {
            dto_error("failed to find target %04x\n", target);
            return OVERLAY_RESULT_TARGET_INVALID;
        }
    }

    if (target_node == NULL) {
        target_path =
                ufdt_node_get_fdt_prop_data_by_name(frag_node, "target-path", NULL);
        if (target_path == NULL) {
            return OVERLAY_RESULT_MISSING_TARGET;
        }

        target_node = ufdt_get_node_by_path(tree, target_path);
        if (target_node == NULL) {
            dto_error("failed to find target-path %s\n", target_path);
            return OVERLAY_RESULT_TARGET_PATH_INVALID;
        }
    }

    overlay_node = ufdt_node_get_node_by_path(frag_node, "__overlay__");
    if (overlay_node == NULL) {
        dto_error("missing __overlay__ sub-node\n");
        return OVERLAY_RESULT_MISSING_OVERLAY;
    }

    bool result = ufdt_verify_overlay_node(target_node, overlay_node, tree, overlay_tree);

    if (!result) {
        dto_error("failed to verify overlay node %s to target %s\n",
                  ufdt_node_name(overlay_node), ufdt_node_name(target_node));
        return OVERLAY_RESULT_VERIFY_FAIL;
    }

    return OVERLAY_RESULT_OK;
}

/*
 * verify each fragment in overlay.
 */
static int ufdt_overlay_verify_fragments(struct ufdt *final_tree,
                                         struct ufdt *overlay_tree) {
    enum overlay_result err;
    struct ufdt_node **it;
    for_each_node(it, overlay_tree->root) {
        err = static_cast<enum overlay_result>(ufdt_verify_fragment(final_tree, overlay_tree,
                                                                    *it));
        if (err == OVERLAY_RESULT_VERIFY_FAIL) {
            return -1;
        }
    }
    return 0;
}

static int ufdt_overlay_verify(struct ufdt *final_tree, struct ufdt *overlay_tree,
                               struct ufdt_node_pool *pool) {
    if (ufdt_overlay_do_fixups_and_combine(final_tree, overlay_tree, pool) < 0) {
        dto_error("failed to perform fixups in overlay\n");
        return -1;
    }

    if (ufdt_overlay_verify_fragments(final_tree, overlay_tree) < 0) {
        dto_error("failed to apply fragments\n");
        return -1;
    }

    return 0;
}

int ufdt_verify_dtbo(struct fdt_header* final_fdt_header,
                     size_t final_fdt_size, void* overlay_fdtp, size_t overlay_size) {
    const size_t min_fdt_size = 8;
    struct ufdt_node_pool pool;
    struct ufdt* final_tree = nullptr;
    struct ufdt* overlay_tree = nullptr;
    int result = 1;

    if (final_fdt_header == NULL) {
        goto fail;
    }

    if (overlay_size < sizeof(struct fdt_header)) {
        dto_error("Overlay_length %zu smaller than header size %zu\n",
                  overlay_size, sizeof(struct fdt_header));
        goto fail;
    }

    if (overlay_size < min_fdt_size || overlay_size != fdt_totalsize(overlay_fdtp)) {
        dto_error("Bad overlay size!\n");
        goto fail;
    }
    if (final_fdt_size < min_fdt_size || final_fdt_size != fdt_totalsize(final_fdt_header)) {
        dto_error("Bad fdt size!\n");
        goto fail;
    }

    ufdt_node_pool_construct(&pool);
    final_tree = ufdt_from_fdt(final_fdt_header, final_fdt_size, &pool);
    overlay_tree = ufdt_from_fdt(overlay_fdtp, overlay_size, &pool);

    result = ufdt_overlay_verify(final_tree, overlay_tree, &pool);
    ufdt_destruct(overlay_tree, &pool);
    ufdt_destruct(final_tree, &pool);
    ufdt_node_pool_destruct(&pool);
fail:
    return result;
}
