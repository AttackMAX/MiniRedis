#pragma once

#include <stddef.h>
#include <stdint.h>


// hashtable node, should be embedded into the payload
struct HNode {
    HNode *next = NULL;  //链表法解决哈希冲突
    uint64_t hcode = 0;  //哈希值
};

// a simple fixed-sized hashtable
struct HTab {  //一个固定大小的哈希表
    HNode **tab = NULL; // 指向哈希槽数组的指针。
    size_t mask = 0;    // 数组大小减 1，用于快速计算哈希槽位置。
    size_t size = 0;    // 当前表中存储的键数量。
};

// the real hashtable interface.
// it uses 2 hashtables for progressive rehashing.
struct HMap {
    /*
        通过两个哈希表实现渐进式reharsh
    */
    HTab newer;  //当前使用的哈希表。
    HTab older;  //正在迁移的旧哈希表。
    size_t migrate_pos = 0; //记录迁移进度。
};

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void   hm_insert(HMap *hmap, HNode *node);
HNode *hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void   hm_clear(HMap *hmap);
size_t hm_size(HMap *hmap);
