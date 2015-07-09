#include <mongo.h>

scale_t *get_signatures(mongo *conn);


int read_config() {
    mongo conn[1];
    int status = mongo_client(conn, "127.0.0.1", 27017);

    if (status != MONGO_OK) {
        switch (conn->err) {
            case MONGO_CONN_NO_SOCKET:
                printf("no socket\n");
                exit(EXIT_FAILURE);
            case MONGO_CONN_FAIL:
                printf("connection failed\n");
                exit(EXIT_FAILURE);
            case MONGO_CONN_NOT_MASTER:
                printf("not master\n");
                exit(EXIT_FAILURE);
        }
    }

    scale_t *scales = get_signatures(&conn);
    printf("%d signatures found\n", sizeof(scales) / sizeof(scales[0]));

    mongo_destroy(conn);

    return (0);
}

scale_t *get_signatures(mongo *conn) {
    scale_t *scales;

    bson query[1];
    mongo_cursor cursor[1];

    bson_init(query);
    bson_finish(query);

    mongo_cursor_init(cursor, conn, "1101.scales");
    mongo_cursor_set_query(cursor, query);

    int num_scales = 0;
    while (mongo_cursor_next(cursor) == MONGO_OK) {
        num_scales++;
        scales = (scale_t *) realloc(scales, sizeof(scale_t) * num_scales);

        bson_iterator iterator[1];
        bson b[1];
        bson_init(b);
        int i;
        if (bson_find(iterator, mongo_cursor_bson(cursor), "name")) {
            printf("name: %s, scale: ", bson_iterator_string(iterator));
            strcpy(scales[num_scales - 1].description, bson_iterator_string(iterator));
        }
        if (bson_find(iterator, mongo_cursor_bson(cursor), "scale")) {
            int scale = bson_iterator_int(iterator);
            scales[num_scales - 1].mask = scale;
            for (i = 11; i >= 0; i--) {
                printf("%s", (scale >> i) & 1 ? "1" : "0");
            }
            printf("\n");
        }
    }

    bson_destroy(query);
    mongo_cursor_destroy(cursor);

    return scales;
}
