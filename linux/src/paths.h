/**
 * @file paths.h
 * @brief XDG-aware path helpers and data-file discovery for the Linux port.
 */
#ifndef CATIME_LINUX_PATHS_H
#define CATIME_LINUX_PATHS_H

#include <stddef.h>

/** Config directory: $XDG_CONFIG_HOME/catime or ~/.config/catime. */
const char *paths_config_dir(void);

/** Data directory for mutable per-user state: $XDG_DATA_HOME/catime. */
const char *paths_data_dir(void);

/** Ensure the config/data directories exist. */
void paths_ensure_dirs(void);

/**
 * Join two path components into @p out.
 * @return 0 on success, -1 if the result would not fit.
 */
int paths_join(char *out, size_t out_size, const char *a, const char *b);

/** Ensure the parent directory of @p path exists. */
int paths_ensure_parent_dir(const char *path);

/**
 * Resolve a bundled read-only data file (relative to a data root), searching:
 *   1. $XDG_DATA_HOME/catime/<rel>
 *   2. <exe_dir>/../share/catime/<rel>
 *   3. /usr/local/share/catime/<rel>
 *   4. /usr/share/catime/<rel>
 *   5. <exe_dir>/../../<rel>            (source-tree dev: e.g. resource/...)
 *   6. <exe_dir>/../../resource/<rel>
 * @return pointer to a static buffer holding the first existing path, or NULL.
 */
const char *paths_find_data(const char *rel);

#endif /* CATIME_LINUX_PATHS_H */
