#include <uhttp/api.h>
#include <uhttp/server.h>

uhttp_response index_handler(uhttp_high_request *req, void *ctx){ (void)req;
    uhttp_response resp;
    uhttp_create_response(
        &resp, 200, "OK",
        "Hello world", strlen("Hello world"),
        HTTP_TEXT_PLAIN
    );

    return resp;
}

int main(int argc, char *argv[]){
    if (argc != 3){
        fprintf(stderr, "usage: %s IP PORT\n", argv[0]);
        return 1;
    }

    naddr_t bind = ln_uniq(argv[1], atoi(argv[2]));
    printf("[uhttp] binding on %s:%u\n", ln_gip(&bind), ln_gport(&bind));

    uhttp_serv serv;
    uhttp_server_create(&serv, bind, NULL);

    uhttp_set_route(&serv, "/", index_handler);

    uhttp_server_poll_routes(&serv);
    uhttp_server_end(&serv);
}

// int main(int argc, char *argv[]){
//     if (argc != 3){
//         fprintf(stderr, "usage: %s IP PORT\n", argv[0]);
//         return 1;
//     }

//     naddr_t bind = ln_uniq(argv[1], atoi(argv[2]));
//     printf("[uhttp] binding on %s:%u\n", ln_gip(&bind), ln_gport(&bind));

//     uhttp_serv  serv;
//     uhttp_serv *ptr = &serv;

//     uhttp_server_create(&serv, bind);
//     uhttp_server_run   (&ptr);

//     while (uhttp_server_isrunning(&serv)){
//         int r = uhttp_wait_requests(&serv, 500);
//         if (r <= 0) continue;

//         uhttp_high_request req;
//         while (0 == uhttp_request_iterate(&serv, &req)){
//             uhttp_response resp;
//             uhttp_create_response(&resp, 200, "OK", "Hello world", strlen("Hello world"), HTTP_TEXT_PLAIN);

//             uhttp_send_response(&serv, &resp, &req);
//         }
//     }

//     uhttp_server_end(&serv);
// }
