#include <assert.h>
#include <stdlib.h>     // calloc(), free()
#include "hashtable.h"


// 初始化一个固定大小的哈希表。
static void h_init(HTab *htab, size_t n) {
    assert(n > 0 && ((n - 1) & n) == 0);  // 确保 n 是 2 的幂（通过 mask 快速计算槽位置）。
    htab->tab = (HNode **)calloc(n, sizeof(HNode *));
    htab->mask = n - 1;
    htab->size = 0;
}

// 将节点插入到哈希表中对应的槽位（链表头部）。
static void h_insert(HTab *htab, HNode *node) {
    size_t pos = node->hcode & htab->mask;
    HNode *next = htab->tab[pos];
    node->next = next;
    htab->tab[pos] = node;
    htab->size++;
}

// hashtable look up subroutine.
// Pay attention to the return value. It returns the address of
// the parent pointer that owns the target node,
// which can be used to delete the target node.
// 查找目标节点，返回指向目标节点的指针。
static HNode **h_lookup(HTab *htab, HNode *key, bool (*eq)(HNode *, HNode *)) {
    if (!htab->tab) {
        return NULL;
    }

    size_t pos = key->hcode & htab->mask;
    HNode **from = &htab->tab[pos];     // incoming pointer to the target
    for (HNode *cur; (cur = *from) != NULL; from = &cur->next) {
        if (cur->hcode == key->hcode && eq(cur, key)) {
            return from;                // may be a node, may be a slot
        }
    }
    return NULL;
}

// 从链表中移除目标节点。
static HNode *h_detach(HTab *htab, HNode **from) {
    HNode *node = *from;    // the target node
    *from = node->next;     // 移除节点
    htab->size--;
    return node;
}

const size_t k_rehashing_work = 128;    // constant work

// 每次迁移固定数量的节点
// 避免一次性重哈希导致性能抖动。
// 每次插入或查找时，顺便迁移部分节点，分摊重哈希成本。
static void hm_help_rehashing(HMap *hmap) {
    size_t nwork = 0;
    while (nwork < k_rehashing_work && hmap->older.size > 0) {
        // find a non-empty slot
        HNode **from = &hmap->older.tab[hmap->migrate_pos];
        if (!*from) {
            hmap->migrate_pos++;
            continue;   // empty slot
        }
        // move the first list item to the newer table
        h_insert(&hmap->newer, h_detach(&hmap->older, from));
        nwork++;
    }
    // discard the old table if done
    if (hmap->older.size == 0 && hmap->older.tab) {
        free(hmap->older.tab);
        hmap->older = HTab{};
    }
}


// 当负载因子超过阈值时，触发重哈希。
static void hm_trigger_rehashing(HMap *hmap) {
    assert(hmap->older.tab == NULL);
    // (newer, older) <- (new_table, newer)
    hmap->older = hmap->newer;
    h_init(&hmap->newer, (hmap->newer.mask + 1) * 2);
    hmap->migrate_pos = 0;
}

/*
    在 newer 和 older 表中查找目标节点。
    优先查找 newer 表，若未找到再查找 older 表。
    确保在迁移过程中数据一致性。
*/ 
HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
    hm_help_rehashing(hmap);
    HNode **from = h_lookup(&hmap->newer, key, eq);
    if (!from) {
        from = h_lookup(&hmap->older, key, eq);
    }
    return from ? *from : NULL;
}

const size_t k_max_load_factor = 8;

/*
    将节点插入到 newer 表，并触发重哈希。
    始终插入到 newer 表，避免影响迁移过程。
    当负载因子超过阈值时，触发渐进式重哈希。
*/
void hm_insert(HMap *hmap, HNode *node) {
    if (!hmap->newer.tab) {
        h_init(&hmap->newer, 4);    // initialize it if empty
    }
    h_insert(&hmap->newer, node);   // always insert to the newer table

    if (!hmap->older.tab) {         // check whether we need to rehash
        size_t shreshold = (hmap->newer.mask + 1) * k_max_load_factor;
        if (hmap->newer.size >= shreshold) {
            hm_trigger_rehashing(hmap);
        }
    }
    hm_help_rehashing(hmap);        // migrate some keys
}

/*
    从 newer 或 older 表中删除目标节点。
    优先删除 newer 表中的节点，若未找到再删除 older 表中的节点。
    确保迁移过程中删除操作的正确性。
*/

HNode *hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
    hm_help_rehashing(hmap);
    if (HNode **from = h_lookup(&hmap->newer, key, eq)) {
        return h_detach(&hmap->newer, from);
    }
    if (HNode **from = h_lookup(&hmap->older, key, eq)) {
        return h_detach(&hmap->older, from);
    }
    return NULL;
}


// 释放 newer 和 older 表的内存。
void hm_clear(HMap *hmap) {
    free(hmap->newer.tab);
    free(hmap->older.tab);
    *hmap = HMap{};
}


// 返回 newer 和 older 表中节点的总数
size_t hm_size(HMap *hmap) {
    return hmap->newer.size + hmap->older.size;
}