#include <linux/fs.h>
#include <linux/msdos_fs.h>

#include "caformat.h"
#include "caformat-util.h"

const char *ca_format_type_name(uint64_t u) {

        switch (u) {

        case CA_FORMAT_HELLO:
                return "hello";

        case CA_FORMAT_ENTRY:
                return "entry";

        case CA_FORMAT_USER:
                return "user";

        case CA_FORMAT_GROUP:
                return "group";

        case CA_FORMAT_SYMLINK:
                return "symlink";

        case CA_FORMAT_DEVICE:
                return "device";

        case CA_FORMAT_PAYLOAD:
                return "payload";

        case CA_FORMAT_GOODBYE:
                return "goodbye";

        case CA_FORMAT_INDEX:
                return "index";

        case CA_FORMAT_TABLE:
                return "table";
        }

        return NULL;
}

static const struct {
        const char *name;
        uint64_t feature;
} with_feature_map[] = {
        { "16bit-uids",       CA_FORMAT_WITH_16BIT_UIDS       },
        { "32bit-uids",       CA_FORMAT_WITH_32BIT_UIDS       },
        { "user-names",       CA_FORMAT_WITH_USER_NAMES       },
        { "sec-time",         CA_FORMAT_WITH_SEC_TIME         },
        { "usec-time",        CA_FORMAT_WITH_USEC_TIME        },
        { "nsec-time",        CA_FORMAT_WITH_NSEC_TIME        },
        { "2sec-time",        CA_FORMAT_WITH_2SEC_TIME        },
        { "read-only",        CA_FORMAT_WITH_READ_ONLY        },
        { "permissions",      CA_FORMAT_WITH_PERMISSIONS      },
        { "symlinks",         CA_FORMAT_WITH_SYMLINKS         },
        { "device-nodes",     CA_FORMAT_WITH_DEVICE_NODES     },
        { "fifos",            CA_FORMAT_WITH_FIFOS            },
        { "sockets",          CA_FORMAT_WITH_SOCKETS          },
        { "flag-hidden",      CA_FORMAT_WITH_FLAG_HIDDEN      },
        { "flag-system",      CA_FORMAT_WITH_FLAG_SYSTEM      },
        { "flag-archive",     CA_FORMAT_WITH_FLAG_ARCHIVE     },
        { "flag-append",      CA_FORMAT_WITH_FLAG_APPEND      },
        { "flag-noatime",     CA_FORMAT_WITH_FLAG_NOATIME     },
        { "flag-compr",       CA_FORMAT_WITH_FLAG_COMPR       },
        { "flag-nocow",       CA_FORMAT_WITH_FLAG_NOCOW       },
        { "flag-nodump",      CA_FORMAT_WITH_FLAG_NODUMP      },
        { "flag-dirsync",     CA_FORMAT_WITH_FLAG_DIRSYNC     },
        { "flag-immutable",   CA_FORMAT_WITH_FLAG_IMMUTABLE   },
        { "flag-sync",        CA_FORMAT_WITH_FLAG_SYNC        },
        { "flag-nocomp",      CA_FORMAT_WITH_FLAG_NOCOMP      },
        { "flag-projinherit", CA_FORMAT_WITH_FLAG_PROJINHERIT },
        { "best",             CA_FORMAT_WITH_BEST             },
        { "unix",             CA_FORMAT_WITH_UNIX             },
        { "fat",              CA_FORMAT_WITH_FAT              },
        { "chattr",           CA_FORMAT_WITH_CHATTR           },
};

int ca_with_feature_flags_parse_one(const char *name, uint64_t *ret) {
        size_t i;

        for (i = 0; i < ELEMENTSOF(with_feature_map); i++)
                if (streq(with_feature_map[i].name, name)) {
                        *ret = with_feature_map[i].feature;
                        return 0;
                }

        return -ENXIO;
}

int ca_with_feature_flags_format(uint64_t features, char **ret) {
        char *s = NULL;
        size_t i;

        for (i = 0; i < ELEMENTSOF(with_feature_map); i++) {
                uint64_t f;

                if (features == 0)
                        break;

                f = with_feature_map[i].feature;

                if ((features & f) != f)
                        continue;

                if (!strextend(&s, s ? " " : "", with_feature_map[i].name, NULL)) {
                        free(s);
                        return -ENOMEM;
                }

                features &= ~f;
        }

        if ((features & ~CA_FORMAT_RESPECT_FLAG_NODUMP) != 0) {
                free(s);
                return -EINVAL;
        }

        *ret = s;
        return 0;
}

int ca_feature_flags_normalize(uint64_t flags, uint64_t *ret) {
        if (!ret)
                return -EINVAL;

        if ((flags & ~CA_FORMAT_FEATURE_FLAGS_MAX) != 0)
                return -EOPNOTSUPP;

        if (flags & CA_FORMAT_WITH_32BIT_UIDS)
                flags &= ~CA_FORMAT_WITH_16BIT_UIDS;

        if (flags & CA_FORMAT_WITH_NSEC_TIME)
                flags &= ~(CA_FORMAT_WITH_USEC_TIME|CA_FORMAT_WITH_SEC_TIME|CA_FORMAT_WITH_2SEC_TIME);
        if (flags & CA_FORMAT_WITH_USEC_TIME)
                flags &= ~(CA_FORMAT_WITH_SEC_TIME|CA_FORMAT_WITH_2SEC_TIME);
        if (flags & CA_FORMAT_WITH_SEC_TIME)
                flags &= ~CA_FORMAT_WITH_2SEC_TIME;

        if (flags & CA_FORMAT_WITH_PERMISSIONS)
                flags &= ~CA_FORMAT_WITH_READ_ONLY;

        if (flags & CA_FORMAT_RESPECT_FLAG_NODUMP)
                flags &= ~CA_FORMAT_WITH_FLAG_NODUMP;

        *ret = flags;
        return 0;
}

int ca_feature_flags_time_granularity_nsec(uint64_t flags, uint64_t *ret) {
        uint64_t granularity;

        if ((flags & ~CA_FORMAT_FEATURE_FLAGS_MAX) != 0)
                return -EOPNOTSUPP;
        if (!ret)
                return -EINVAL;

        if (flags & CA_FORMAT_WITH_NSEC_TIME)
                granularity = 1;
        else if (flags & CA_FORMAT_WITH_USEC_TIME)
                granularity = 1000;
        else if (flags & CA_FORMAT_WITH_SEC_TIME)
                granularity = 1000000000;
        else if (flags & CA_FORMAT_WITH_2SEC_TIME)
                granularity = 2000000000;
        else
                return -ENODATA;

        *ret = granularity;
        return 0;
}

static const struct {
        uint64_t feature_flag;
        unsigned chattr_flag;
} chattr_map[] = {
        { CA_FORMAT_WITH_FLAG_APPEND,      FS_APPEND_FL      },
        { CA_FORMAT_WITH_FLAG_NOATIME,     FS_NOATIME_FL     },
        { CA_FORMAT_WITH_FLAG_COMPR,       FS_COMPR_FL       },
        { CA_FORMAT_WITH_FLAG_NOCOW,       FS_NOCOW_FL       },
        { CA_FORMAT_WITH_FLAG_NODUMP,      FS_NODUMP_FL      },
        { CA_FORMAT_WITH_FLAG_DIRSYNC,     FS_DIRSYNC_FL     },
        { CA_FORMAT_WITH_FLAG_IMMUTABLE,   FS_IMMUTABLE_FL   },
        { CA_FORMAT_WITH_FLAG_SYNC,        FS_SYNC_FL        },
        { CA_FORMAT_WITH_FLAG_NOCOMP,      FS_NOCOMP_FL      },
        { CA_FORMAT_WITH_FLAG_PROJINHERIT, FS_PROJINHERIT_FL },
};

uint64_t ca_feature_flags_from_chattr(unsigned flags) {
        uint64_t f = 0;
        size_t i;

        for (i = 0; i < ELEMENTSOF(chattr_map); i++)
                if (flags & chattr_map[i].chattr_flag)
                        f |= chattr_map[i].feature_flag;

        return f;
}

unsigned ca_feature_flags_to_chattr(uint64_t flags) {
        unsigned f = 0;
        size_t i;

        for (i = 0; i < ELEMENTSOF(chattr_map); i++) {
                if (flags & chattr_map[i].feature_flag)
                        f |= chattr_map[i].chattr_flag;
        }

        return f;
}

static const struct {
        uint64_t feature_flag;
        uint32_t fat_flag;
} fat_attrs_map[] = {
        { CA_FORMAT_WITH_FLAG_HIDDEN,  ATTR_HIDDEN },
        { CA_FORMAT_WITH_FLAG_SYSTEM,  ATTR_SYS    },
        { CA_FORMAT_WITH_FLAG_ARCHIVE, ATTR_ARCH   },
};

uint64_t ca_feature_flags_from_fat_attrs(uint32_t flags) {
        uint64_t f = 0;
        size_t i;

        for (i = 0; i < ELEMENTSOF(fat_attrs_map); i++)
                if (flags & fat_attrs_map[i].fat_flag)
                        f |= fat_attrs_map[i].feature_flag;

        return f;
}

uint32_t ca_feature_flags_to_fat_attrs(uint64_t flags) {
        uint32_t f = 0;
        size_t i;

        for (i = 0; i < ELEMENTSOF(fat_attrs_map); i++) {
                if (flags & fat_attrs_map[i].feature_flag)
                        f |= fat_attrs_map[i].fat_flag;
        }

        return f;
}

uint64_t ca_feature_flags_from_magic(statfs_f_type_t magic) {

        /* Returns the set of features we know a specific file system type provides. Ideally the kernel would let us
         * know this, but this is Linux and hence we have crappy interfaces. */

        switch (magic) {

        case MSDOS_SUPER_MAGIC:
                return
                        CA_FORMAT_WITH_2SEC_TIME|
                        CA_FORMAT_WITH_READ_ONLY|
                        CA_FORMAT_WITH_FLAG_HIDDEN|
                        CA_FORMAT_WITH_FLAG_SYSTEM|
                        CA_FORMAT_WITH_FLAG_ARCHIVE;

        case EXT2_SUPER_MAGIC:
                return
                        CA_FORMAT_WITH_16BIT_UIDS|
                        CA_FORMAT_WITH_32BIT_UIDS|
                        CA_FORMAT_WITH_USER_NAMES|
                        CA_FORMAT_WITH_SEC_TIME|
                        CA_FORMAT_WITH_USEC_TIME|
                        CA_FORMAT_WITH_NSEC_TIME|
                        CA_FORMAT_WITH_2SEC_TIME|
                        CA_FORMAT_WITH_READ_ONLY|
                        CA_FORMAT_WITH_PERMISSIONS|
                        CA_FORMAT_WITH_SYMLINKS|
                        CA_FORMAT_WITH_DEVICE_NODES|
                        CA_FORMAT_WITH_FIFOS|
                        CA_FORMAT_WITH_SOCKETS|
                        CA_FORMAT_WITH_FLAG_APPEND|
                        CA_FORMAT_WITH_FLAG_NOATIME|
                        CA_FORMAT_WITH_FLAG_NODUMP|
                        CA_FORMAT_WITH_FLAG_DIRSYNC|
                        CA_FORMAT_WITH_FLAG_IMMUTABLE|
                        CA_FORMAT_WITH_FLAG_SYNC;

        case XFS_SUPER_MAGIC:
                return
                        CA_FORMAT_WITH_16BIT_UIDS|
                        CA_FORMAT_WITH_32BIT_UIDS|
                        CA_FORMAT_WITH_USER_NAMES|
                        CA_FORMAT_WITH_SEC_TIME|
                        CA_FORMAT_WITH_USEC_TIME|
                        CA_FORMAT_WITH_NSEC_TIME|
                        CA_FORMAT_WITH_2SEC_TIME|
                        CA_FORMAT_WITH_READ_ONLY|
                        CA_FORMAT_WITH_PERMISSIONS|
                        CA_FORMAT_WITH_SYMLINKS|
                        CA_FORMAT_WITH_DEVICE_NODES|
                        CA_FORMAT_WITH_FIFOS|
                        CA_FORMAT_WITH_SOCKETS|
                        CA_FORMAT_WITH_FLAG_APPEND|
                        CA_FORMAT_WITH_FLAG_NOATIME|
                        CA_FORMAT_WITH_FLAG_NODUMP|
                        CA_FORMAT_WITH_FLAG_IMMUTABLE|
                        CA_FORMAT_WITH_FLAG_SYNC;

        case BTRFS_SUPER_MAGIC:
                return
                        CA_FORMAT_WITH_16BIT_UIDS|
                        CA_FORMAT_WITH_32BIT_UIDS|
                        CA_FORMAT_WITH_USER_NAMES|
                        CA_FORMAT_WITH_SEC_TIME|
                        CA_FORMAT_WITH_USEC_TIME|
                        CA_FORMAT_WITH_NSEC_TIME|
                        CA_FORMAT_WITH_2SEC_TIME|
                        CA_FORMAT_WITH_READ_ONLY|
                        CA_FORMAT_WITH_PERMISSIONS|
                        CA_FORMAT_WITH_SYMLINKS|
                        CA_FORMAT_WITH_DEVICE_NODES|
                        CA_FORMAT_WITH_FIFOS|
                        CA_FORMAT_WITH_SOCKETS|
                        CA_FORMAT_WITH_FLAG_APPEND|
                        CA_FORMAT_WITH_FLAG_NOATIME|
                        CA_FORMAT_WITH_FLAG_COMPR|
                        CA_FORMAT_WITH_FLAG_NOCOW|
                        CA_FORMAT_WITH_FLAG_NODUMP|
                        CA_FORMAT_WITH_FLAG_DIRSYNC|
                        CA_FORMAT_WITH_FLAG_IMMUTABLE|
                        CA_FORMAT_WITH_FLAG_SYNC|
                        CA_FORMAT_WITH_FLAG_NOCOMP;

        case TMPFS_MAGIC:
        default:
                /* For now, let's assume that tmpfs defines the baseline of what Linux file systems support */
                return
                        CA_FORMAT_WITH_16BIT_UIDS|
                        CA_FORMAT_WITH_32BIT_UIDS|
                        CA_FORMAT_WITH_USER_NAMES|
                        CA_FORMAT_WITH_SEC_TIME|
                        CA_FORMAT_WITH_USEC_TIME|
                        CA_FORMAT_WITH_NSEC_TIME|
                        CA_FORMAT_WITH_2SEC_TIME|
                        CA_FORMAT_WITH_READ_ONLY|
                        CA_FORMAT_WITH_PERMISSIONS|
                        CA_FORMAT_WITH_SYMLINKS|
                        CA_FORMAT_WITH_DEVICE_NODES|
                        CA_FORMAT_WITH_FIFOS|
                        CA_FORMAT_WITH_SOCKETS;
        }
}
