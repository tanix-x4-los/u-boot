// SPDX-License-Identifier: GPL-2.0+
/* Derived from the Vontar GPT publisher; X4 validation and boot-source gates. */
#include <common.h>
#include <mmc.h>
#include <part.h>
#include <u-boot/crc.h>
#include <malloc.h>
#include <memalign.h>
#include <partition_table.h>
#include <emmc_partitions.h>
#include <amlogic/x4_boot.h>

static void x4_part_uuid(const char *name, char *uuid, int len)
{
	u32 h = 2166136261u;
	const unsigned char *p;

	for (p = (const unsigned char *)name; *p; p++)
		h = (h ^ *p) * 16777619u;
	snprintf(uuid, len, "6f%06x-a4b0-4c2d-8e1f-%012x",
		 h & 0xffffff, h);
}

static int x4_write_primary_gpt(struct blk_desc *dev_desc,
				     disk_partition_t *parts, int n)
{
	gpt_header *gpt_h = NULL;
	gpt_entry *gpt_e = NULL;
	int ret = -1;
	int hsize, esize;
	lbaint_t pte_blks;
	char disk_guid[] = "a0453c8e-6c14-4b6e-9c1a-0000c0a4a4a4";
	ALLOC_CACHE_ALIGN_BUFFER_PAD(legacy_mbr, p_mbr, 1, dev_desc->blksz);

	hsize = PAD_TO_BLOCKSIZE(sizeof(*gpt_h), dev_desc);
	gpt_h = malloc_cache_aligned(hsize);
	esize = PAD_TO_BLOCKSIZE(GPT_ENTRY_NUMBERS * sizeof(*gpt_e), dev_desc);
	gpt_e = malloc_cache_aligned(esize);
	if (!gpt_h || !gpt_e)
		goto out;
	memset(gpt_h, 0, hsize);
	memset(gpt_e, 0, esize);

	if (gpt_fill_header(dev_desc, gpt_h, disk_guid, n))
		goto out;
	/* Cover the whole user area. There is no backup header at the end. */
	gpt_h->last_usable_lba = cpu_to_le64(dev_desc->lba - 1);
	if (gpt_fill_pte(dev_desc, gpt_h, gpt_e, parts, n))
		goto out;

	if (blk_dread(dev_desc, 0, 1, p_mbr) != 1)
		goto out;
	memset((char *)p_mbr + MSDOS_MBR_BOOT_CODE_SIZE, 0,
	       sizeof(*p_mbr) - MSDOS_MBR_BOOT_CODE_SIZE);
	p_mbr->signature = MSDOS_MBR_SIGNATURE;
	p_mbr->partition_record[0].sys_ind = EFI_PMBR_OSTYPE_EFI_GPT;
	p_mbr->partition_record[0].start_sect = 1;
	p_mbr->partition_record[0].nr_sects = (u32)dev_desc->lba - 1;

	gpt_h->partition_entry_array_crc32 = cpu_to_le32(
		crc32(0, (unsigned char *)gpt_e,
		      le32_to_cpu(gpt_h->num_partition_entries) *
		      le32_to_cpu(gpt_h->sizeof_partition_entry)));
	gpt_h->header_crc32 = 0;
	gpt_h->header_crc32 = cpu_to_le32(
		crc32(0, (unsigned char *)gpt_h,
		      le32_to_cpu(gpt_h->header_size)));

	pte_blks = BLOCK_CNT(le32_to_cpu(gpt_h->num_partition_entries) *
			     sizeof(gpt_entry), dev_desc);
	if (blk_dwrite(dev_desc, 0, 1, p_mbr) != 1 ||
	    blk_dwrite(dev_desc, 1, 1, gpt_h) != 1 ||
	    blk_dwrite(dev_desc,
		       (lbaint_t)le64_to_cpu(gpt_h->partition_entry_lba),
		       pte_blks, gpt_e) != pte_blks)
		goto out;
	ret = 0;
out:
	free(gpt_e);
	free(gpt_h);
	return ret;
}

static int x4_publish_gpt(void)
{
	struct blk_desc *dev_desc;
	struct partitions *pt;
	disk_partition_t *parts;
	gpt_entry *pte = NULL;
	struct mmc *mmc;
	int i, j, n, count, ret = CMD_RET_FAILURE;
	lbaint_t last;

	ALLOC_CACHE_ALIGN_BUFFER_PAD(gpt_header, gpt_head, 1, 512);

	mmc = find_mmc_device(CONFIG_FASTBOOT_FLASH_MMC_DEV);
	if (!mmc || mmc_init(mmc))
		return CMD_RET_FAILURE;
	dev_desc = mmc_get_blk_desc(mmc);
	if (!dev_desc || dev_desc->blksz != 512 || dev_desc->hwpart != 0 ||
	    dev_desc->lba < 0x100000 || dev_desc->lba > 0xffffffffULL)
		return CMD_RET_FAILURE;

	if (is_gpt_valid(dev_desc, GPT_PRIMARY_PARTITION_TABLE_LBA,
			 gpt_head, &pte) == 1) {
		free(pte);
		puts("GPT already valid; leaving it unchanged\n");
		return CMD_RET_SUCCESS;
	}

	/* A damaged existing primary GPT must not trigger conversion. */
	if (blk_dread(dev_desc, GPT_PRIMARY_PARTITION_TABLE_LBA, 1,
		      gpt_head) != 1)
		return CMD_RET_FAILURE;
	if (!memcmp(gpt_head, "EFI PART", 8)) {
		puts("X4: existing GPT header is invalid; refusing migration\n");
		return CMD_RET_FAILURE;
	}

	pt = aml_ept_table(&count);
	if (!pt || count < 1 || count > GPT_ENTRY_NUMBERS)
		return CMD_RET_FAILURE;

	parts = calloc(count, sizeof(*parts));
	if (!parts)
		return CMD_RET_FAILURE;

	last = dev_desc->lba - 1;
	for (i = 0, n = 0; i < count; i++) {
		lbaint_t start = pt[i].offset / dev_desc->blksz;
		lbaint_t size;

		/* Disabled zero-length entries (stock cache) have no GPT extent. */
		if (!pt[i].name[0] || !pt[i].size)
			continue;
		if (!memchr(pt[i].name, 0, sizeof(pt[i].name)))
			goto out_parts;
		/* LBA 0-33 belong to the protective MBR, header and entries. */
		if (start < 34) {
			if (strcmp(pt[i].name, "bootloader"))
				goto out_parts;
			printf("gpt: skip %s at LBA %lu\n", pt[i].name,
			       (unsigned long)start);
			continue;
		}
		if (pt[i].offset % dev_desc->blksz || start > last ||
		    !memchr(pt[i].name, 0, sizeof(pt[i].name)))
			goto out_parts;
		if (pt[i].size == (uint64_t)-1)
			size = last + 1 - start;
		else {
			if (pt[i].size % dev_desc->blksz)
				goto out_parts;
			size = pt[i].size / dev_desc->blksz;
		}
		if (!size || size > last + 1 - start)
			goto out_parts;
		for (j = 0; j < n; j++) {
			if (!strcmp(pt[i].name, (char *)parts[j].name) ||
			    (start < parts[j].start + parts[j].size &&
			     parts[j].start < start + size))
				goto out_parts;
		}

		parts[n].start = start;
		parts[n].size = size;
		parts[n].blksz = dev_desc->blksz;
		strncpy((char *)parts[n].name, pt[i].name,
			sizeof(parts[n].name) - 1);
#if CONFIG_IS_ENABLED(PARTITION_UUIDS)
		x4_part_uuid(pt[i].name, parts[n].uuid,
				 sizeof(parts[n].uuid));
#endif
		n++;
	}

	if (n > 0 && !x4_write_primary_gpt(dev_desc, parts, n)) {
		/* Validate the written primary header and entry-array CRCs. */
		if (is_gpt_valid(dev_desc, GPT_PRIMARY_PARTITION_TABLE_LBA,
				 gpt_head, &pte) == 1) {
			free(pte);
			part_init(dev_desc);
			printf("gpt: published and verified %d entries\n", n);
			ret = CMD_RET_SUCCESS;
		} else {
			puts("X4: GPT readback validation failed\n");
		}
	}
	else
		printf("gpt: not published (%d entries)\n", n);
out_parts:
	free(parts);
	return ret;
}

/* GPT overwrites the user-area BL2; require a hardware eMMC boot. */
int x4_migrate_gpt(void)
{
	u32 status = readl(SYSCTRL_SEC_STATUS_REG2);
	u32 source = (status >> 4) & 0xf;
	u32 copy = status & 0xf;

	if (x4_usb_ram_boot()) {
		puts("X4: USB RAM boot; skipping automatic GPT migration\n");
		return CMD_RET_SUCCESS;
	}
	if (source != BOOT_ID_EMMC || (copy != 1 && copy != 2)) {
		puts("GPT requires an eMMC hardware-partition boot\n");
		return CMD_RET_FAILURE;
	}
	return x4_publish_gpt();
}

static int do_x4_gpt(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	if (argc != 2 || strcmp(argv[1], "publish"))
		return CMD_RET_USAGE;
	if (x4_usb_ram_boot())
		return CMD_RET_FAILURE;
	return x4_migrate_gpt();
}

U_BOOT_CMD(x4_gpt, 2, 0, do_x4_gpt,
	   "publish primary GPT from the existing Amlogic partition map",
	   "publish - requires hardware boot copy 1 or 2; preserves partition data");
