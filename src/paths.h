#ifndef _PATHS_H_
#define _PATHS_H_

/* Realloc the base path and add the suffix to it, "/"s handled nicely */
char *paths_append(char *base, const char *suffix);

/* Get the ~/.config/bytenuts directory */
char *paths_bnconf_dir();

/* Get the default bytenuts config filepath */
char *paths_bnconf_default();

/* Generate the path to the command<idx> file */
char *paths_command_file(int idx);

/* Generate a logfile path of the form ~/.config/bytenuts/<prefix>[.<pid>].log
 * PID only inserted if >0 */
char *paths_logfile(const char *prefix, long long pid);

#endif /* _PATHS_H_ */
