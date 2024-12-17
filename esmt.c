// SPDX-License-Identifier: GPL-2.0

#ifndef __UBOOT__
#include <linux/device.h>
#include <linux/kernel.h>
#endif
#include <linux/mtd/spinand.h>

#define SPINAND_MFR_ESMT_C8		0xc8
#define SPINAND_MFR_ESMT_2C		0x2c
#define SPINAND_MFR_ESMT_8C		0x8c

#define ESMT_STATUS_ECC_MASK		GENMASK(7, 4)
#define ESMT_STATUS_ECC_NO_BITFLIPS	    (0 << 4)
#define ESMT_STATUS_ECC_1TO3_BITFLIPS	(1 << 4)
#define ESMT_STATUS_ECC_4TO6_BITFLIPS	(3 << 4)
#define ESMT_STATUS_ECC_7TO8_BITFLIPS	(5 << 4)

static SPINAND_OP_VARIANTS(quadio_read_cache_variants,
		SPINAND_PAGE_READ_FROM_CACHE_QUADIO_OP(0, 2, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X4_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_DUALIO_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X2_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(true, 0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(false, 0, 1, NULL, 0));

static SPINAND_OP_VARIANTS(x4_read_cache_variants,
		SPINAND_PAGE_READ_FROM_CACHE_X4_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X2_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(true, 0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(false, 0, 1, NULL, 0));

static SPINAND_OP_VARIANTS(x4_write_cache_variants,
		SPINAND_PROG_LOAD_X4(true, 0, NULL, 0),
		SPINAND_PROG_LOAD(true, 0, NULL, 0));

static SPINAND_OP_VARIANTS(x4_update_cache_variants,
		SPINAND_PROG_LOAD_X4(false, 0, NULL, 0),
		SPINAND_PROG_LOAD(false, 0, NULL, 0));

/*
 * F50L1G41LB & F50D1G41LB & F50L1G41LC OOB spare area map (64 bytes)
 *
 * Bad Block Markers
 * filled by HW and kernel                 Reserved
 *   |                 +-----------------------+-----------------------+
 *   |                 |                       |                       |
 *   |                 |    OOB free data Area |non ECC protected      |
 *   |   +-------------|-----+-----------------|-----+-----------------|-----+
 *   |   |             |     |                 |     |                 |     |
 * +-|---|----------+--|-----|--------------+--|-----|--------------+--|-----|--------------+
 * | |   | section0 |  |     |    section1  |  |     |    section2  |  |     |    section3  |
 * +-v-+-v-+---+----+--v--+--v--+-----+-----+--v--+--v--+-----+-----+--v--+--v--+-----+-----+
 * |   |   |   |    |     |     |     |     |     |     |     |     |     |     |     |     |
 * |0:1|2:3|4:7|8:15|16:17|18:19|20:23|24:31|32:33|34:35|36:39|40:47|48:49|50:51|52:55|56:63|
 * |   |   |   |    |     |     |     |     |     |     |     |     |     |     |     |     |
 * +---+---+-^-+--^-+-----+-----+--^--+--^--+-----+-----+--^--+--^--+-----+-----+--^--+--^--+
 *           |    |                |     |                 |     |                 |     |
 *           |    +----------------|-----+-----------------|-----+-----------------|-----+
 *           |             ECC Area|(Main + Spare) - filled|by ESMT NAND HW        |
 *           |                     |                       |                       |
 *           +---------------------+-----------------------+-----------------------+
 *                         OOB ECC protected Area - not used due to
 *                         partial programming from some filesystems
 *                             (like JFFS2 with cleanmarkers)
 */

/*
 * F50L2G41KA OOB spare area map (128 bytes)
 *
 * Bad Block Markers
 * filled by HW and kernel
 *   |    
 *   |             OOB free data area               ECC area (main+spare)                                  
 *   |             with ECC protected                filled by NAND HW 
 *   |   +-----------+-----------+-----------+     +-----+-----+-------+
 *   |   |           |           |           |     |     |     |       |
 * +-v-+-v--+-----+--v--+-----+--v--+-----+--v--+--v--+--v--+--v---+---v---+
 * |   |    |     |     |     |     |     |     |     |     |      |       |
 * |0:3|4:15|16:19|20:31|32:35|36:47|48:51|52:63|64:79|80:95|96:111|112:127|
 * |   |    |     |     |     |     |     |     |     |     |      |       |
 * +---+----+--^--+-----+--^--+-----+--^--+-----+-----+-----+------+-------+
 *             |           |           |
 *             +-----------+-----------+
 *                     Reserved
 */

/*
 * F50L2G41XA OOB spare area map (128 bytes)
 *
 * Bad Block Markers
 * filled by HW and kernel
 *   |                                            OOB free data area     ECC area (main+spare)
 *   |  OOB free data Area non ECC protected      with ECC protected      filled by NAND HW
 *   |   +---+-----+-----+-----+-----+-----+     +-----+-----+-----+     +-----+-----+------+
 *   |   |   |     |     |     |     |     |     |     |     |     |     |     |     |      |
 * +-v-+-v-+-v--+--v--+--v--+--v--+--v--+--v--+--v--+--v--+--v--+--v--+--v--+--v--+--v---+--v----+
 * |   |   |    |     |     |     |     |     |     |     |     |     |     |     |      |       |
 * |0:3|4:7|8:12|13:15|16:19|20:23|24:27|28:31|32:39|40:47|48:55|56:63|64:79|80:95|96:111|112:127|
 * |   |   |    |     |     |     |     |     |     |     |     |     |     |     |      |       |
 * +-^-+-^-+-^--+--^--+--^--+--^--+--^--+--^--+--^--+--^--+--^--+--^--+--^--+--^--+--^---+---^---+
 *   |   |   |     |     |     |     |     |     |     |     |     |     |     |     |       |
 *   +---|---|-----|-----+-----|-----|-----|-----+-----|-----|-----|-----+     |     |       |
 *       |   |     |  Spare 0  |     |     |           |     |     |           |     |       |
 *       +---|-----|-----------+-----|-----|-----------+-----|-----|-----------+     |       |
 *           |     |  Spare 1        |     |                 |     |                 |       |
 *           +-----|-----------------+-----|-----------------+-----|-----------------+       |
 *                 |  Spare 2              |                       |                         |
 *                 +-----------------------+-----------------------+-------------------------+
 *                    Spare 3
 */

#define ESMT_OOB_SECTION_COUNT			4
#define ESMT_OOB_SECTION_SIZE(nand) \
	(nanddev_per_page_oobsize(nand) / ESMT_OOB_SECTION_COUNT)
#define ESMT_OOB_FREE_SIZE(nand) \
	(ESMT_OOB_SECTION_SIZE(nand) / 2)
#define ESMT_OOB_ECC_SIZE(nand) \
	(ESMT_OOB_SECTION_SIZE(nand) - ESMT_OOB_FREE_SIZE(nand))
#define ESMT_OOB_BBM_SIZE			2
#define ESMT_OOB_BBM_SIZE_128byte	4

static int f50l1g41lb_ooblayout_ecc(struct mtd_info *mtd, int section,
				    struct mtd_oob_region *region)
{
	struct nand_device *nand = mtd_to_nanddev(mtd);

	if (section >= ESMT_OOB_SECTION_COUNT)
		return -ERANGE;

	region->offset = section * ESMT_OOB_SECTION_SIZE(nand) +
			 ESMT_OOB_FREE_SIZE(nand);
	region->length = ESMT_OOB_ECC_SIZE(nand);

	return 0;
}

static int f50l1g41lb_ooblayout_free(struct mtd_info *mtd, int section,
				     struct mtd_oob_region *region)
{
	struct nand_device *nand = mtd_to_nanddev(mtd);

	if (section >= ESMT_OOB_SECTION_COUNT)
		return -ERANGE;

	/*
	 * Reserve space for bad blocks markers (section0) and
	 * reserved bytes (sections 1-3)
	 */
	region->offset = section * ESMT_OOB_SECTION_SIZE(nand) + 2;

	/* Use only 2 non-protected ECC bytes per each OOB section */
	region->length = 2;

	return 0;
}

static const struct mtd_ooblayout_ops f50l1g41lb_ooblayout = {
	.ecc = f50l1g41lb_ooblayout_ecc,
	.rfree = f50l1g41lb_ooblayout_free,
};

static const struct mtd_ooblayout_ops f50l1g41lc_ooblayout = {
	.ecc = f50l1g41lb_ooblayout_ecc,
	.rfree = f50l1g41lb_ooblayout_free,
};

static int f50l2g41ka_ooblayout_ecc(struct mtd_info *mtd, int section,
				    struct mtd_oob_region *region)
{
	struct nand_device *nand = mtd_to_nanddev(mtd);

	if (section >= ESMT_OOB_SECTION_COUNT)
		return -ERANGE;

	region->offset = 64 + section * 16;
	region->length = 16;

	return 0;
}

static int f50l2g41ka_ooblayout_free(struct mtd_info *mtd, int section,
				     struct mtd_oob_region *region)
{
	struct nand_device *nand = mtd_to_nanddev(mtd);

	if (section >= ESMT_OOB_SECTION_COUNT)
		return -ERANGE;

	/*
	 * Reserve space for bad blocks markers (section0) and
	 * reserved bytes (sections 1-3)
	 */
	region->offset = section * 16 + 4;

	/* Use only 12 ECC protected bytes per each OOB section */
	region->length = 12;

	return 0;
}

static const struct mtd_ooblayout_ops f50l2g41ka_ooblayout = {
	.ecc = f50l2g41ka_ooblayout_ecc,
	.rfree = f50l2g41ka_ooblayout_free,
};


static int f50l2g41xa_ooblayout_ecc(struct mtd_info *mtd, int section,
				    struct mtd_oob_region *region)
{
	struct nand_device *nand = mtd_to_nanddev(mtd);

	if (section >= ESMT_OOB_SECTION_COUNT)
		return -ERANGE;

	region->offset = 64 + section * 8;
	region->length = 16;

	return 0;
}

static int f50l2g41xa_ooblayout_free(struct mtd_info *mtd, int section,
				     struct mtd_oob_region *region)
{
	struct nand_device *nand = mtd_to_nanddev(mtd);

	if (section >= ESMT_OOB_SECTION_COUNT)
		return -ERANGE;

	/*
	 * used space for oob free area with ECC protected
	 */
	region->offset = 32 + section * 8;

	/* Use only 8 ECC protected bytes per each OOB section */
	region->length = 8;

	return 0;
}

static const struct mtd_ooblayout_ops f50l2g41xa_ooblayout = {
	.ecc = f50l2g41xa_ooblayout_ecc,
	.rfree = f50l2g41xa_ooblayout_free,
};

static int esmt_8_ecc_get_status(struct spinand_device *spinand,
				   u8 status)
{
	switch (status & ESMT_STATUS_ECC_MASK) {
	case STATUS_ECC_NO_BITFLIPS:
		return 0;

	case STATUS_ECC_UNCOR_ERROR:
		return -EBADMSG;

	case ESMT_STATUS_ECC_1TO3_BITFLIPS:
		return 3;

	case ESMT_STATUS_ECC_4TO6_BITFLIPS:
		return 6;

	case ESMT_STATUS_ECC_7TO8_BITFLIPS:
		return 8;

	default:
		break;
	}

	return -EINVAL;
}

static const struct spinand_info esmt_c8_spinand_table[] = {
	SPINAND_INFO("F50L1G41LB", /*1Gb, 3.3V, SPI NAND*/
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_ADDR, 0x01),
		     NAND_MEMORG(1, 2048, 64, 64, 1024, 20, 1, 1, 1),
		     NAND_ECCREQ(1, 512),
		     SPINAND_INFO_OP_VARIANTS(&x4_read_cache_variants,
					      &x4_write_cache_variants,
					      &x4_update_cache_variants),
		     0,
		     SPINAND_ECCINFO(&f50l1g41lb_ooblayout, NULL)),
	SPINAND_INFO("F50D1G41LB", /*1Gb, 1.8V, SPI NAND*/
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_ADDR, 0x11),
		     NAND_MEMORG(1, 2048, 64, 64, 1024, 20, 1, 1, 1),
		     NAND_ECCREQ(1, 512),
		     SPINAND_INFO_OP_VARIANTS(&x4_read_cache_variants,
					      &x4_write_cache_variants,
					      &x4_update_cache_variants),
		     0,
		     SPINAND_ECCINFO(&f50l1g41lb_ooblayout, NULL)),

	SPINAND_INFO("F50L2G41KA", /*2Gb, 3.3V, SPI NAND*/
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_ADDR, 0x41),
		     NAND_MEMORG(1, 2048, 128, 64, 2048, 40, 1, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&x4_read_cache_variants,
					      &x4_write_cache_variants,
					      &x4_update_cache_variants),
		     0,
		     SPINAND_ECCINFO(&f50l2g41ka_ooblayout,esmt_8_ecc_get_status)),
};

static const struct spinand_info esmt_2c_spinand_table[] = {
	SPINAND_INFO("F50L2G41XA", /*2Gb, 3.3V, SPI NAND*/
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_ADDR, 0x24),
		     NAND_MEMORG(1, 2048, 128, 64, 2048, 40, 2, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&x4_read_cache_variants,
					      &x4_write_cache_variants,
					      &x4_update_cache_variants),
		     0,
		     SPINAND_ECCINFO(&f50l2g41xa_ooblayout,esmt_8_ecc_get_status)),
};

static const struct spinand_info esmt_8c_spinand_table[] = {
	SPINAND_INFO("F50L1G41LC", /*1Gb, 3.3V, SPI NAND*/
		     SPINAND_ID(SPINAND_READID_METHOD_OPCODE_ADDR, 0x2C),
		     NAND_MEMORG(1, 2048, 64, 64, 1024, 20, 1, 1, 1),
		     NAND_ECCREQ(1, 512),
		     SPINAND_INFO_OP_VARIANTS(&quadio_read_cache_variants,
					      &x4_write_cache_variants,
					      &x4_update_cache_variants),
		     0,
		     SPINAND_ECCINFO(&f50l1g41lb_ooblayout, NULL)),
};

static const struct spinand_manufacturer_ops esmt_spinand_manuf_ops = {
};

const struct spinand_manufacturer esmt_c8_spinand_manufacturer = {
	.id = SPINAND_MFR_ESMT_C8,
	.name = "ESMT",
	.chips = esmt_c8_spinand_table,
	.nchips = ARRAY_SIZE(esmt_c8_spinand_table),
	.ops = &esmt_spinand_manuf_ops,
};

const struct spinand_manufacturer esmt_2c_spinand_manufacturer = {
	.id = SPINAND_MFR_ESMT_2C,
	.name = "ESMT",
	.chips = esmt_2c_spinand_table,
	.nchips = ARRAY_SIZE(esmt_2c_spinand_table),
	.ops = &esmt_spinand_manuf_ops,
};

const struct spinand_manufacturer esmt_8c_spinand_manufacturer = {
	.id = SPINAND_MFR_ESMT_8C,
	.name = "ESMT",
	.chips = esmt_8c_spinand_table,
	.nchips = ARRAY_SIZE(esmt_8c_spinand_table),
	.ops = &esmt_spinand_manuf_ops,
};