#include "libmx.h"

static unsigned long long int read_chars = 0;

static int get_fd(int old_fd) {
    const char *base_path = "/proc/self/fd/";
    char *fd_str = mx_itoa(old_fd);
    char *full_path = mx_strjoin(base_path, fd_str);
    int fd = open(full_path, O_RDWR);
    free(fd_str);
    free(full_path);
    return fd;
}

static int get_delim_index(const char *buffer, size_t len, char delim) {
    if (!buffer) return -1;
    for (size_t i = 0; i < len; ++i) if (buffer[i] == delim) return (int)i;
    return -1;
}

static int shift_fd(int fd) {
    char tc;  // shift new FD if something was read in previous call
    if (read_chars > 0)
        for (unsigned long long int i = 0; i <= read_chars; ++i)
            if (read(fd, &tc, 1) < 0) return -2;
    return 0;
}

static int eol_found(unsigned long long int new_static,
                     t_read_line_data *data, char *buffer, int size) {
    read_chars = new_static;
    mx_strncpy(data->end, buffer, size);
    data->end += size;
    return 1;
}

static int resize_memory(t_read_line_data *data, int *size) {
    (*size)++;
    unsigned long long int len = data->end - data->st;
    data->st = mx_realloc(data->st, (*size) * data->b_s);
    if (data->st == NULL) return -2;
    mx_memset(data->st + ((*size) - 1) * data->b_s, 0, data->b_s);
    data->end = data->st + len;
    return 0;
}

static int process_line(int fd, t_read_line_data *data, char delim) {
    int EOL = 0;
    int size = 1;  // measured in buffers
    char *buffer = mx_strnew((int)data->b_s);

    while (!EOL) {
        size_t result;
        if ((result = read(fd, buffer, data->b_s)) < 0) {
            free(buffer);
            return -2;
        }
        int dem_ind = get_delim_index(buffer, result, delim);
        // EOF і немає роздільника
        if (result < data->b_s && dem_ind < 0)
            EOL = eol_found(0, data, buffer, (int)result);
            // Знайдено роздільник
        else if (dem_ind >= 0)
            EOL = eol_found(read_chars + dem_ind, data, buffer, dem_ind);
            // Немає роздільника, копіюємо весь буфер
        else eol_found(read_chars + result, data, buffer, (int)result);
        // Розширюємо пам'ять, якщо це необхідно
        if (data->end - data->st > (long long int)((size - 1) * data->b_s))
            if (resize_memory(data, &size) < 0) return -2;
    }
    free(buffer);
    return EOL;
}

/**
 * @NAME Read line a.k.a. Mr. Big
 * @DESCRIPTION Create a function that reads the line
 * from the given file into the lineptr until it reaches a delimiter character.
 * The delimiter must not be returned with lineptr,
 * and the function must stop when it reaches the End Of File (EOF).\n
 * A line is a sequence of characters before a delimiter.\n
 * The function: \n
 * - works correctly with any file descriptor fd. \n
 * - works correctly with any positive integer bufsize.
 * bufsize must be passed to the function read as a parameter nbytes. \n
 * - can read all data from the given file until EOF, one line per call. \n
 * - may contain a single static variable
 * while global variables are still forbidden.\n
 * - may have undefined behavior when reading from a binary file.\n
 * @RETURN returns the number of bytes that are written into lineptr. \n
 * returns -1 if EOF is reached and there is nothing to write in lineptr. \n
 * returns -2 in case of errors or if fd is invalid.
 * @note \n
 */
int mx_read_line(char **lineptr, size_t buf_size, char delim, const int fd) {
    if (!lineptr || buf_size == 0) return -2;
    int newFD = get_fd(fd);
    if (newFD < 0 || shift_fd(newFD) < 0) return -2;
    t_read_line_data data = {mx_memset(mx_strnew((int)buf_size),
                             0, buf_size), NULL, buf_size};
    data.end = data.st;
    int EOL = process_line(newFD, &data, delim);
    if (EOL < 0) {
        free(data.st);
        return -2;
    }
    int res = (int)(data.end - data.st + 1);
    if (!*lineptr) *lineptr = mx_strnew(res);
    else if (mx_strlen(*lineptr) < res)
        *lineptr = mx_realloc(*lineptr, res);
    mx_strncpy(*lineptr, data.st, res);
    free(data.st);
    close(newFD);
    return res - 1;
}
