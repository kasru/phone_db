#include "phone_db.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <inttypes.h>

int phone_key_from_hex(phone_key_t *key, const char *hex) {
    if (!key || !hex) return -1;
    memset(key->bytes, 0, PHONE_KEY_LEN);
    int len = (int)strlen(hex);
    if (len > PHONE_NUM_MAX_HEX) return -1;
    /* hex-строка выровнена по правому краю в 20 ниблов (10 байт) */
    int nibble_start = PHONE_NUM_MAX_HEX - len;
    for (int i = 0; i != len; ++i) {
        int nibble_idx = nibble_start + i + 1;
        int byte_idx = nibble_idx / 2;
        int shift = (nibble_idx % 2 == 0) ? 4 : 0;
        char c = hex[i];
        uint8_t val;
        if (c >= '0' && c <= '9') val = c - '0';
        else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;
        else return -1;
        key->bytes[byte_idx] |= (val << shift);
    }
    return 0;
}

int phone_key_to_hex(const phone_key_t *key, char *buf, size_t bufsize) {
    if (bufsize < PHONE_NUM_MAX_HEX + 1) return -1;
    for (int i = 0; i != PHONE_NUM_MAX_HEX; ++i) {
        int nibble_idx = i + 1;
        int byte_idx = nibble_idx / 2;
        int shift = (nibble_idx % 2 == 0) ? 4 : 0;
        uint8_t val = (key->bytes[byte_idx] >> shift) & 0x0F;
        buf[i] = (val < 10) ? ('0' + val) : ('A' + val - 10);
    }
    buf[PHONE_NUM_MAX_HEX] = '\0';
    return PHONE_NUM_MAX_HEX;
}

int phone_key_compare(const phone_key_t *a, const phone_key_t *b) {
    return memcmp(a->bytes, b->bytes, PHONE_KEY_LEN);
}

int phone_key_is_empty(const phone_key_t *key) {
    for (int i = 0; i != PHONE_KEY_LEN; ++i)
        if (key->bytes[i] != 0) return 0;
    return 1;
}

int phone_db_init(phone_db_t *db, size_t capacity, size_t comment_buf_cap,
                  size_t pending_capacity) {
    /* Защита от утечки памяти при повторном вызове */
    phone_db_destroy(db);
    memset(db, 0, sizeof(*db));

    db->capacity = capacity ? capacity : DEFAULT_CAPACITY;
    db->records = calloc(db->capacity, sizeof(phone_record_t));
    if (!db->records) return -1;

    db->comment_buf_cap = comment_buf_cap ? comment_buf_cap : DEFAULT_COMMENT_BUF_CAP;
    db->comment_buf = malloc(db->comment_buf_cap);
    if (!db->comment_buf) {
        free(db->records);
        db->records = NULL;
        return -1;
    }

    db->pending_capacity = pending_capacity ? pending_capacity : DEFAULT_PENDING_CAPACITY;
    db->pending = calloc(db->pending_capacity, sizeof(pending_entry_t));
    if (!db->pending) {
        free(db->records);
        db->records = NULL;
        free(db->comment_buf);
        db->comment_buf = NULL;
        return -1;
    }

    db->magic = PHONE_DB_MAGIC;
    return 0;
}

void phone_db_destroy(phone_db_t *db) {
    if (!db || db->magic != PHONE_DB_MAGIC) return;
    free(db->records);
    free(db->comment_buf);
    free(db->pending);
    memset(db, 0, sizeof(*db));
}

int phone_db_reset(phone_db_t *db) {
    size_t cap = (db->magic == PHONE_DB_MAGIC) ? db->capacity : 0;
    size_t cbuf = (db->magic == PHONE_DB_MAGIC) ? db->comment_buf_cap : 0;
    size_t pcap = (db->magic == PHONE_DB_MAGIC) ? db->pending_capacity : 0;

    return phone_db_init(db, cap, cbuf, pcap);
}

/**
 * @brief Увеличивает ёмкость массива записей при необходимости.
 *
 * Если текущее количество записей достигает capacity,
 * массив удваивается через realloc.
 *
 * @param db указатель на БД телефонов.
 * @return 0 при успехе, -1 при ошибке выделения памяти.
 */
static int ensure_capacity(phone_db_t *db) {
    if (db->count < db->capacity) return 0;
    if (db->capacity > SIZE_MAX / 2) return -1;
    size_t new_cap = db->capacity * 2;
    phone_record_t *p = realloc(db->records, new_cap * sizeof(phone_record_t));
    if (!p) return -1;
    db->records = p;
    db->capacity = new_cap;
    return 0;
}

/**
 * @brief Увеличивает ёмкость массива отложенных операций.
 *
 * Если текущее количество отложенных операций достигает
 * pending_capacity, массив удваивается через realloc.
 *
 * @param db указатель на БД телефонов.
 * @return 0 при успехе, -1 при ошибке выделения памяти.
 */
static int ensure_pending_capacity(phone_db_t *db) {
    if (db->pending_count < db->pending_capacity) return 0;
    if (db->pending_capacity > SIZE_MAX / 2) return -1;
    size_t new_cap = db->pending_capacity * 2;
    pending_entry_t *p = realloc(db->pending, new_cap * sizeof(pending_entry_t));
    if (!p) return -1;
    db->pending = p;
    db->pending_capacity = new_cap;
    return 0;
}

/**
 * @brief Разбирает строку даты формата YYYY-MM-DD.
 *
 * Преобразует строку в количество дней с эпохи (1970-01-01).
 * Возвращает 0 для пустых или некорректных строк.
 *
 * @param s строка с датой в формате YYYY-MM-DD.
 * @return количество дней с эпохи, или 0 при ошибке.
 */
static uint32_t parse_expiry(const char *s) {
    /* Проверка входных данных: NULL или пустая строка -> 0 ("без срока") */
    if (!s || *s == '\0') return 0;
    int64_t y, m, d;
    /* Парсинг строки YYYY-MM-DD. Если формат неверен -> UINT32_MAX (ошибка) */
    if (sscanf(s, "%" SCNd64 "-%" SCNd64 "-%" SCNd64, &y, &m, &d) != 3) return UINT32_MAX;
    /* Проверка допустимости значений месяца и дня */
    if (m < 1 || m > 12 || d < 1 || d > 31) return UINT32_MAX;

    /* Преобразование YYYY-MM-DD в дни с эпохи (1970-01-01).
     * Алгоритм: https://howardhinnant.github.io/date_algorithms.html */
    if (m <= 2) {
        /* Январь/февраль считаем как 13/14 месяц предыдущего года.
         * Это упрощает вычисление дня года */
        y--;
        m += 12;
    }
    int64_t era = y / 400;                      /* Эпоха = 400-летний цикл */
    int64_t yoe = y - era * 400;                /* Год внутри эпохи (0..399) */
    int64_t doy = (153 * (m - 3) + 2) / 5 + d - 1;  /* День года (0..364) */
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;  /* День внутри эпохи */
    int64_t days = era * 146097 + doe - 719468;
    if (days < 0 || days > (int64_t)UINT32_MAX) return UINT32_MAX;
    return (uint32_t)days;
}

/**
 * @brief Добавляет комментарий в основной буфер комментариев.
 *
 * Копирует строку комментария в буфер с автоматическим
 * расширением при необходимости. Возвращает смещение и длину
 * для записи в структуру phone_record_t.
 *
 * @param db указатель на БД телефонов.
 * @param comment строка комментария.
 * @param len длина строки комментария.
 * @param [out] offset_out смещение в буфере комментариев.
 * @param [out] len_out длина комментария в буфере.
 * @return 0 при успехе, -1 при ошибке выделения памяти.
 */
static int add_comment(phone_db_t *db, const char *comment, size_t len,
                        uint32_t *offset_out, uint16_t *len_out) {
    if (len > UINT16_MAX) return -1;
    *offset_out = (uint32_t)db->comment_buf_size;
    *len_out = (uint16_t)len;
    if (db->comment_buf_size + len > db->comment_buf_cap) {
        if (db->comment_buf_cap > SIZE_MAX / 2) return -1;
        size_t new_cap = db->comment_buf_cap * 2;
        while (new_cap < db->comment_buf_size + len) new_cap *= 2;
        char *p = realloc(db->comment_buf, new_cap);
        if (!p) return -1;
        db->comment_buf = p;
        db->comment_buf_cap = new_cap;
    }
    if (len > 0)
        memcpy(db->comment_buf + db->comment_buf_size, comment, len);
    db->comment_buf_size += len;
    return 0;
}

static int record_cmp(const void *a, const void *b) {
    return phone_key_compare(&((const phone_record_t*)a)->key,
                             &((const phone_record_t*)b)->key);
}

/**
 * @brief Сортирует массив записей по ключу телефона.
 *
 * Использует qsort с компаратором record_cmp.
 * Сортировка необходима для бинарного поиска.
 *
 * @param db указатель на БД телефонов.
 */
static void sort_records(phone_db_t *db) {
    qsort(db->records, db->count, sizeof(phone_record_t), record_cmp);
}

/**
 * @brief Ищет отложенную операцию по ключу телефона.
 *
 * Линейный поиск по массиву pending операций.
 * Возвращает индекс найденной операции.
 *
 * @param db указатель на БД телефонов.
 * @param key ключ телефона для поиска.
 * @param [out] idx_out индекс найденной операции.
 * @return 1 если найдено, 0 если не найдено.
 */
static int find_pending(const phone_db_t *db, const phone_key_t *key, size_t *idx_out) {
    for (size_t i = 0; i != db->pending_count; ++i) {
        if (phone_key_compare(&db->pending[i].record.key, key) == 0) {
            *idx_out = i;
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Бинарный поиск ключа в отсортированном массиве записей.
 *
 * Массив должен быть предварительно отсортирован по ключу.
 * Возвращает индекс найденного или позицию для вставки.
 *
 * @param arr отсортированный массив записей.
 * @param count количество записей в массиве.
 * @param key ключ телефона для поиска.
 * @param [out] idx_out индекс найденной записи или позиция для вставки.
 * @return 1 если ключ найден, 0 если не найден.
 */
static int binary_search(const phone_record_t *arr, size_t count,
                          const phone_key_t *key, size_t *idx_out) {
    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int cmp = phone_key_compare(&arr[mid].key, key);
        if (cmp == 0) {
            *idx_out = mid;
            return 1;
        } else if (cmp < 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    *idx_out = lo;
    return 0;
}

int phone_db_load_csv(phone_db_t *db, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return -1;

    /* Пересоздать БД, сохраняя capacity */
    if (phone_db_reset(db) != 0) {
        fclose(f);
        return -1;
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, f)) != -1) {
        /* Удалить завершающий перенос строки */
        size_t len = (size_t)linelen;
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';
        if (len == 0) continue;

        /* Разбор: телефон;комментарий;срок_действия */
        char *sep1 = strchr(line, CSV_SEP);
        if (!sep1) continue;
        *sep1 = '\0';
        char *phone = line;
        char *rest = sep1 + 1;

        char *sep2 = strchr(rest, CSV_SEP);
        char *comment = rest;
        char *expiry_str = "";
        if (sep2) {
            *sep2 = '\0';
            expiry_str = sep2 + 1;
        }

        phone_key_t key;
        if (phone_key_from_hex(&key, phone) != 0) continue;

        if (ensure_capacity(db) != 0) {
            phone_db_reset(db);
            free(line);
            fclose(f);
            return -1;
        }

        uint32_t co = 0;
        uint16_t cl = 0;
        if (add_comment(db, comment, strlen(comment), &co, &cl) != 0) {
            phone_db_reset(db);
            free(line);
            fclose(f);
            return -1;
        }

        phone_record_t *rec = &db->records[db->count++];
        rec->key = key;
        rec->comment_offset = co;
        rec->comment_len = cl;
        rec->expiry = parse_expiry(expiry_str);
    }
    free(line);
    fclose(f);

    sort_records(db);

    /* Дедупликация — удаление дубликатов ключей (оставляем первый) */
    if (db->count > 1) {
        size_t dst = 0;
        for (size_t src = 1; src != db->count; ++src) {
            if (phone_key_compare(&db->records[dst].key, &db->records[src].key) != 0) {
                ++dst;
                db->records[dst] = db->records[src];
            }
        }
        db->count = dst + 1;
    }

    return 0;
}

int phone_db_apply_increment(phone_db_t *db, char op, const char *number,
                              const char *comment, const char *expiry) {
    if (op != OP_ADD && op != OP_DELETE && op != OP_UPDATE) return -1;

    phone_key_t key;
    if (phone_key_from_hex(&key, number) != 0) return -1;

    if (ensure_pending_capacity(db) != 0) return -1;

    /* Проверка существования ключа для UPDATE и DELETE */
    if (op == OP_UPDATE || op == OP_DELETE) {
        size_t idx;
        int found = binary_search(db->records, db->count, &key, &idx) &&
                    phone_key_compare(&db->records[idx].key, &key) == 0;
        if (!found) {
            size_t pidx;
            found = find_pending(db, &key, &pidx);
        }
        if (!found) return -1;
    }

    uint32_t co = 0;
    uint16_t cl = 0;
    if (comment && *comment) {
        if (add_comment(db, comment, strlen(comment), &co, &cl) != 0)
            return -1;
    }

    /* Если для этого ключа уже есть отложенная операция — заменить её.
     * Примечание: старые байты комментария в comment_buf не переиспользуются
     * (append-only буфер). Утечка ограничена размером pending-операций. */
    size_t existing;
    if (find_pending(db, &key, &existing)) {
        pending_entry_t *e = &db->pending[existing];
        e->record.comment_offset = co;
        e->record.comment_len = cl;
        e->record.expiry = parse_expiry(expiry);
        if (!(e->op == OP_ADD && op == OP_UPDATE)) {
            e->op = (uint8_t)op;
        }
        return 0;
    }

    pending_entry_t *e = &db->pending[db->pending_count++];
    e->record.key = key;
    e->record.comment_offset = co;
    e->record.comment_len = cl;
    e->record.expiry = parse_expiry(expiry);
    e->op = (uint8_t)op;
    return 0;
}

int phone_db_lookup(const phone_db_t *db, const phone_key_t *key,
                     const char **comment, uint16_t *comment_len,
                     uint32_t *expiry) {
    /* Сначала проверить отложенные изменения */
    size_t pidx;
    if (find_pending(db, key, &pidx)) {
        const pending_entry_t *e = &db->pending[pidx];
        if (e->op == OP_DELETE) return -1;
        if (comment) *comment = db->comment_buf + e->record.comment_offset;
        if (comment_len) *comment_len = e->record.comment_len;
        if (expiry) *expiry = e->record.expiry;
        return 0;
    }

    /* Бинарный поиск в отсортированном массиве */
    size_t idx;
    if (!binary_search(db->records, db->count, key, &idx))
        return -1;

    const phone_record_t *rec = &db->records[idx];
    if (comment) *comment = db->comment_buf + rec->comment_offset;
    if (comment_len) *comment_len = rec->comment_len;
    if (expiry) *expiry = rec->expiry;
    return 0;
}

/**
 * @brief Элемент временного массива для сохранения в отсортированном порядке.
 *
 * Использует один указатель вместо копирования данных записи.
 * Экономия: 8 байт vs 20+ байт при копии.
 *
 * Все комментарии хранятся в одном буфере comment_buf,
 * поэтому дополнительные указатели не нужны.
 */
typedef struct {
    const phone_record_t *rec;      /**< указатель на запись (ключ + данные) */
} save_entry_t;

/**
 * @brief Компаратор для сортировки элементов save_entry по ключу.
 *
 * Используется в qsort для упорядочивания временного массива
 * указателей на записи по ключу телефона (big-endian).
 *
 * @param a первый элемент (save_entry_t*).
 * @param b второй элемент (save_entry_t*).
 * @return отрицательное, нулевое или положительное значение.
 */
static int save_entry_compare(const void *a, const void *b) {
    const save_entry_t *ea = (const save_entry_t *)a;
    const save_entry_t *eb = (const save_entry_t *)b;
    return phone_key_compare(&ea->rec->key, &eb->rec->key);
}

int phone_db_save_sorted(const phone_db_t *db, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) return -1;

    /* Подсчёт общего количества записей:
     * записи без pending DELETE + новые записи из pending ADD */
    size_t total = db->count;
    for (size_t i = 0; i != db->pending_count; ++i) {
        size_t idx;
        int found = binary_search(db->records, db->count,
                                  &db->pending[i].record.key, &idx);
        if (db->pending[i].op == OP_DELETE) {
            if (found && phone_key_compare(&db->records[idx].key,
                                           &db->pending[i].record.key) == 0)
                total--;
        } else if (db->pending[i].op == OP_ADD) {
            if (!found || phone_key_compare(&db->records[idx].key,
                                            &db->pending[i].record.key) != 0)
                total++;
        }
    }

    /* Пустая БД: записать только заголовок */
    if (total == 0) {
        fclose(f);
        return 0;
    }

    /* Выделение временного массива указателей.
     * Экономия памяти: указатель (8 байт) вместо копии записи (20 байт).
     * Для UPDATE-операций переопределённые поля хранятся отдельно. */
    save_entry_t *tmp_save_entries = malloc(total * sizeof(save_entry_t));
    if (!tmp_save_entries) {
        fclose(f);
        return -1;
    }

    /* Заполнение из основного буфера с учётом pending-операций */
    size_t n = 0;
    for (size_t i = 0; i != db->count; ++i) {
        size_t pidx;
        if (find_pending(db, &db->records[i].key, &pidx)) {
            if (db->pending[pidx].op == OP_DELETE) continue;
            /* UPDATE: указываем на pending-запись (ключи одинаковые) */
            tmp_save_entries[n].rec = &db->pending[pidx].record;
            n++;
        } else {
            /* Без pending: указываем на основную запись */
            tmp_save_entries[n].rec = &db->records[i];
            n++;
        }
    }

    /* Добавление новых записей из pending ADD */
    for (size_t i = 0; i != db->pending_count; ++i) {
        if (db->pending[i].op != OP_ADD) continue;
        size_t idx;
        if (!binary_search(db->records, db->count, &db->pending[i].record.key, &idx) ||
            phone_key_compare(&db->records[idx].key, &db->pending[i].record.key) != 0) {
            /* Новая запись: указываем на pending-запись */
            tmp_save_entries[n].rec = &db->pending[i].record;
            n++;
        }
    }

    /* Сортировка по ключу телефона */
    qsort(tmp_save_entries, n, sizeof(save_entry_t), save_entry_compare);

    /* Запись в файл */
    for (size_t i = 0; i != n; ++i) {
        char hex[PHONE_NUM_MAX_HEX + 1];
        phone_key_to_hex(&tmp_save_entries[i].rec->key, hex, sizeof(hex));
        /* Убираем ведущие нули */
        char *p = hex;
        while (*p == '0' && *(p+1)) p++;

        /* Все комментарии в одном буфере comment_buf */
        fprintf(f, "%s;%.*s;%u\n", p,
                tmp_save_entries[i].rec->comment_len,
                db->comment_buf + tmp_save_entries[i].rec->comment_offset,
                tmp_save_entries[i].rec->expiry);
    }

    free(tmp_save_entries);
    fclose(f);
    return 0;
}

size_t phone_db_memory_usage(const phone_db_t *db) {
    return sizeof(phone_db_t)
         + db->capacity * sizeof(phone_record_t)
         + db->comment_buf_cap
         + db->pending_capacity * sizeof(pending_entry_t);
}
