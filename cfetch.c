#include <ctype.h>
#include <dirent.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#define HOSTNAME_MAX 256
#define BUFFER_SIZE 1024

#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_PURPLE "\033[35m"
#define COLOR_CYAN "\033[36m"
#define COLOR_WHITE "\033[37m"
#define COLOR_BLACK "\033[30m"

#define LOGO_PATH "./logos"
#define MAX_LOGO_LINES 20
#define MAX_LINE_LENGTH 50

void get_distro_name(char *distro) {
  FILE *os_release = fopen("/etc/os-release", "r");
  if (os_release) {
    char line[256];
    while (fgets(line, sizeof(line), os_release)) {
      if (strncmp(line, "PRETTY_NAME=", 11) == 0) {
        char *start = strchr(line, '"');
        char *end = strrchr(line, '"');
        if (start && end) {
          strncpy(distro, start + 1, end - start - 1);
          distro[end - start - 1] = '\0';
          break;
        }
      }
    }
    fclose(os_release);
  }
}

void get_shell_name(char *shell) {
  pid_t ppid = getppid();

  char cmd[256];
  snprintf(cmd, sizeof(cmd), "/proc/%d/cmdline", ppid);

  FILE *f = fopen(cmd, "r");
  if (f) {
    if (fgets(shell, 256, f)) {
      char *last_slash = strrchr(shell, '/');
      if (last_slash) {
        memmove(shell, last_slash + 1, strlen(last_slash + 1) + 1);
      }
      shell[strcspn(shell, "\n")] = 0;
    } else {
      strcpy(shell, "Unknown");
    }
    fclose(f);
  } else {
    strcpy(shell, "Unknown");
  }
}

void get_desktop_environment(char *de) {
  char *current_de = getenv("XDG_CURRENT_DESKTOP");
  char *session_type = getenv("XDG_SESSION_TYPE");
  char *window_manager = getenv("DESKTOP_SESSION");

  if (current_de) {
    strcpy(de, current_de);
  } else if (window_manager) {
    strcpy(de, window_manager);
  } else if (session_type) {
    strcpy(de, session_type);
  } else {
    strcpy(de, "Unknown");
  }
}

// format uptime
void format_uptime(char *uptime_str, long seconds) {
  int days = seconds / (60 * 60 * 24);
  int hours = (seconds % (60 * 60 * 24)) / (60 * 60);
  int minutes = (seconds % (60 * 60)) / 60;

  if (days > 0) {
    sprintf(uptime_str, "%d days, %d hours, %d mins", days, hours, minutes);
  } else if (hours > 0) {
    sprintf(uptime_str, "%d hours, %d mins", hours, minutes);
  } else {
    sprintf(uptime_str, "%d mins", minutes);
  }
}

unsigned long get_cached_memory() {
  FILE *meminfo = fopen("/proc/meminfo", "r");
  unsigned long cached = 0;
  unsigned long buffers = 0; // Add buffers
  unsigned long shmem = 0;
  unsigned long sreclaimable = 0;

  if (meminfo) {
    char line[256];
    while (fgets(line, sizeof(line), meminfo)) {
      if (strncmp(line, "Cached:", 7) == 0) {
        sscanf(line, "Cached: %lu", &cached);
      } else if (strncmp(line, "Buffers:", 8) == 0) {
        sscanf(line, "Buffers: %lu", &buffers);
      } else if (strncmp(line, "Shmem:", 6) == 0) {
        sscanf(line, "Shmem: %lu", &shmem);
      } else if (strncmp(line, "SReclaimable:", 13) == 0) {
        sscanf(line, "SReclaimable: %lu", &sreclaimable);
      }
    }
    fclose(meminfo);
  }
  return (cached + buffers + sreclaimable - shmem) * 1024;
}

void load_distro_logo(const char *distro_name,
                      char logo[MAX_LOGO_LINES][MAX_LINE_LENGTH],
                      int *logo_lines) {
  char logo_file[512];
  char distro_lower[256];
  int i;

  for (i = 0; distro_name[i]; i++) {
    distro_lower[i] = tolower(distro_name[i]);
  }
  distro_lower[i] = '\0';

  char *src = distro_lower;
  char *dst = distro_lower;
  while (*src) {
    if (isalnum(*src)) {
      *dst = *src;
      dst++;
    }
    src++;
  }
  *dst = '\0';

  snprintf(logo_file, sizeof(logo_file), "%s/%s.txt", LOGO_PATH, distro_lower);

  FILE *f = fopen(logo_file, "r");
  if (f == NULL) {
    snprintf(logo_file, sizeof(logo_file), "%s/default.txt", LOGO_PATH);
    f = fopen(logo_file, "r");
    if (f == NULL) {
      *logo_lines = 0;
      return;
    }
  }

  *logo_lines = 0;
  while (fgets(logo[*logo_lines], MAX_LINE_LENGTH, f) &&
         *logo_lines < MAX_LOGO_LINES) {
    logo[*logo_lines][strcspn(logo[*logo_lines], "\n")] = 0;
    (*logo_lines)++;
  }

  fclose(f);
}

int main() {
  struct utsname system_info;
  struct sysinfo mem_info;
  struct passwd *pw = getpwuid(getuid());
  char hostname[HOSTNAME_MAX];
  char distro[256] = "Unknown";
  char shell[256] = "Unknown";
  char cpu_model[BUFFER_SIZE] = "Unknown";
  char de[256] = "Unknown";
  char uptime_str[256];
  char logo[MAX_LOGO_LINES][MAX_LINE_LENGTH];
  int logo_lines = 0;

  uname(&system_info);
  sysinfo(&mem_info);
  gethostname(hostname, HOSTNAME_MAX);
  get_distro_name(distro);
  get_shell_name(shell);
  get_desktop_environment(de);
  format_uptime(uptime_str, mem_info.uptime);

  FILE *cpuinfo = fopen("/proc/cpuinfo", "r");
  if (cpuinfo) {
    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), cpuinfo)) {
      if (strncmp(line, "model name", 10) == 0) {
        char *colon = strchr(line, ':');
        if (colon) {
          strcpy(cpu_model, colon + 2);
          cpu_model[strcspn(cpu_model, "\n")] = 0;
          char *at_symbol = strstr(cpu_model, "@");
          if (at_symbol) {
            *at_symbol = '\0';
          }
          char *cpu_suffix = strstr(cpu_model, "CPU");
          if (cpu_suffix) {
            *cpu_suffix = '\0';
          }
          int len = strlen(cpu_model);
          while (len > 0 && cpu_model[len - 1] == ' ') {
            cpu_model[--len] = '\0';
          }
          break;
        }
      }
    }
    fclose(cpuinfo);
  }

  unsigned long cached_mem = get_cached_memory();
  float total_ram_gb = (float)(mem_info.totalram * mem_info.mem_unit) /
                       (1024.0 * 1024.0 * 1024.0);
  float used_ram = (mem_info.totalram - mem_info.freeram) * mem_info.mem_unit;
  float used_ram_gb = (used_ram - cached_mem) / (1024.0 * 1024.0 * 1024.0);

  load_distro_logo(distro, logo, &logo_lines);

  int max_logo_width = 0;
  for (int i = 0; i < logo_lines; i++) {
    int len = strlen(logo[i]);
    if (len > max_logo_width)
      max_logo_width = len;
  }

  printf("\n");
  int info_start_line = 2;

  for (int i = 0; i < logo_lines || i < 10; i++) {
    if (i < logo_lines) {
      printf("%s\033[1m%-*s%s", COLOR_CYAN, max_logo_width, logo[i],
             COLOR_RESET);
    } else {
      printf("%-*s", max_logo_width, "");
    }
    if (i == 0) {
      printf("  %s\033[1m╭────────────╮%s\n", COLOR_WHITE, COLOR_RESET);
    } else if (i == 1) {
      printf("  %s│%s %s%s%s \033[1mUser:%s    %s│%s %s\033[1m%s@%s%s\n",
             COLOR_WHITE, COLOR_RESET, COLOR_BLUE, "", COLOR_RESET,
             COLOR_RESET, COLOR_WHITE, COLOR_RESET, COLOR_BLUE, pw->pw_name,
             hostname, COLOR_RESET);
    } else if (i == 2) {
      printf("  %s\033[1m├────────────┤%s\n", COLOR_WHITE, COLOR_RESET);
    } else if (i == 3) {
      printf("  %s\033[1m│%s %s%s%s \033[1mDistro:%s  %s\033[1m│%s "
             "%s\033[1m%s%s\n",
             COLOR_WHITE, COLOR_RESET, COLOR_RED, "󰣇", COLOR_RESET,
             COLOR_RESET, COLOR_WHITE, COLOR_RESET, COLOR_RED, distro,
             COLOR_RESET);
    } else if (i == 4) {
      printf("  %s\033[1m│%s %s%s%s \033[1mKernel:%s  %s\033[1m│%s "
             "%s\033[1m%s%s\n",
             COLOR_WHITE, COLOR_RESET, COLOR_GREEN, "", COLOR_RESET,
             COLOR_RESET, COLOR_WHITE, COLOR_RESET, COLOR_GREEN,
             system_info.release, COLOR_RESET);
    } else if (i == 5) {
      printf("  %s\033[1m│%s %s%s%s \033[1mShell:%s   %s\033[1m│%s "
             "%s\033[1m%s%s\n",
             COLOR_WHITE, COLOR_RESET, COLOR_YELLOW, "", COLOR_RESET,
             COLOR_RESET, COLOR_WHITE, COLOR_RESET, COLOR_YELLOW, shell,
             COLOR_RESET);
    } else if (i == 6) {
      printf("  %s\033[1m│%s %s%s%s \033[1mDE/WM:%s   %s\033[1m│%s "
             "%s\033[1m%s%s\n",
             COLOR_WHITE, COLOR_RESET, COLOR_BLUE, "", COLOR_RESET,
             COLOR_RESET, COLOR_WHITE, COLOR_RESET, COLOR_BLUE, de,
             COLOR_RESET);
    } else if (i == 7) {
      printf("  %s\033[1m│%s %s%s%s \033[1mUptime:%s  %s\033[1m│%s "
             "%s\033[1m%s%s\n",
             COLOR_WHITE, COLOR_RESET, COLOR_PURPLE, "", COLOR_RESET,
             COLOR_RESET, COLOR_WHITE, COLOR_RESET, COLOR_PURPLE, uptime_str,
             COLOR_RESET);
    } else if (i == 8) {
      printf("  %s\033[1m│%s %s%s%s \033[1mCPU:%s     %s\033[1m│%s "
             "%s\033[1m%s%s\n",
             COLOR_WHITE, COLOR_RESET, COLOR_CYAN, "", COLOR_RESET,
             COLOR_RESET, COLOR_WHITE, COLOR_RESET, COLOR_CYAN, cpu_model,
             COLOR_RESET);
    } else if (i == 9) {
      /* printf("  %s\033[1m│%s %s%s%s \033[1mMemory:%s  %s\033[1m│%s " */
      /*        "%s\033[1m%.1f GB / %.1f GB%s\n", */
      printf("  %s\033[1m│%s %s%s%s \033[1mMemory:%s  %s\033[1m│%s "
             /*"%s\033[1m%.1f GB / %.1f GB (%.1f%%)%s\n",*/
             "%s\033[1m%.1f GB / %.1f GB %s(%s\033[1m%.1f%%%s)\n",
             COLOR_WHITE, COLOR_RESET, COLOR_GREEN, "", COLOR_RESET,
             COLOR_RESET, COLOR_WHITE, COLOR_RESET, COLOR_GREEN, used_ram_gb,
             total_ram_gb, COLOR_RESET, COLOR_BLUE,
             (used_ram_gb / total_ram_gb) * 100.0, COLOR_RESET);
    } else if (i == 10) {
      printf("  %s\033[1m╰────────────╯%s\n", COLOR_WHITE, COLOR_RESET);
    } else if (i == 11) {
      printf("  %s╭────────────╮%s\n", COLOR_WHITE, COLOR_RESET);
    }

    else if (i == 12) {
      printf("  %s│%s %s%s%s \033[1mColors:%s  %s│%s %s%s  %s%s  "
             "%s%s "
             " %s%s  %s %s %s %s %s %s\n",
             COLOR_WHITE, COLOR_RESET, COLOR_CYAN, "", COLOR_RESET,
             COLOR_RESET, COLOR_WHITE, COLOR_RESET, COLOR_RED, COLOR_RESET,
             COLOR_YELLOW, COLOR_RESET, COLOR_GREEN, COLOR_RESET, COLOR_BLUE,
             COLOR_RESET, COLOR_PURPLE, COLOR_RESET, COLOR_CYAN, COLOR_RESET,
             COLOR_WHITE, COLOR_RESET);
    } else if (i == 13) {
      printf("  %s╰────────────╯%s\n", COLOR_WHITE, COLOR_RESET);
    } else {
      printf("\n");
    }
  }

  return 0;
}
