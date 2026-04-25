/*
 * Biometric Vault — Filesystem Metadata Structures
 *
 * vault.img içindeki verilerin nasıl dizileceğini belirler.
 * Basitlik için başlangıçta "Flat" (tek seviyeli) bir yapı kullanacağız.
 */

#ifndef FS_METADATA_H
#define FS_METADATA_H

#include <stdint.h>

#define FS_MAGIC            0x42564C54  /* "BVLT" (Biometric Vault) */
#define MAX_FILENAME        255
#define MAX_FILES           128         /* Şimdilik maksimum 128 dosya */

/**
 * Superblock: Dosya sisteminin en başındaki (Block 0) genel bilgiler.
 */
typedef struct {
    uint32_t magic;         /* Magic number (BVLT) */
    uint32_t block_size;    /* Genellikle 4096 */
    uint64_t total_blocks;  /* Image içindeki toplam blok sayısı */
    uint32_t inode_count;   /* Şu anki dosya sayısı */
} vault_superblock_t;

/**
 * Inode: Her bir dosyanın bilgilerini tutan yapı.
 */
typedef struct {
    char     filename[MAX_FILENAME];
    uint64_t size;          /* Dosya boyutu (byte) */
    uint64_t start_block;   /* Verinin başladığı blok numarası */
    uint32_t in_use;        /* Bu slot dolu mu? (1/0) */
    uint32_t mode;          /* Dosya izinleri (Permissions) */
    int64_t  ctime;         /* Oluşturulma zamanı */
    int64_t  mtime;         /* Değiştirilme zamanı */
} vault_inode_t;

/* 
 * Layout (vault.img):
 * [ Block 0: Superblock ]
 * [ Block 1-4: Inode Table (128 dosya * ~300 byte = ~38KB) ]
 * [ Block 5...: Data Blocks (Encrypted) ]
 */

#define SUPERBLOCK_BLOCK    0
#define INODE_TABLE_START   1
#define INODE_TABLE_BLOCKS  4
#define DATA_START_BLOCK    (INODE_TABLE_START + INODE_TABLE_BLOCKS)

#endif /* FS_METADATA_H */
