#include <err.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include <cjson/cJSON.h>

#include <nfc/nfc.h>
#include <nfc/nfc-types.h>

#include "squeeze.h"

#define PAUSE_ID "0421ef7a835781"
#define NEXT_ID  "04ada67a835780"
#define PREV_ID  "04319f7a835781"

static nfc_device *pnd = NULL;
static nfc_context *context;
const char _last_serial[32] = {0};
struct timeval _last_time_detection = {0};
char *_current = NULL;

static void stop_polling(int sig)
{
    (void) sig;
    if (pnd != NULL)
    {
        nfc_abort_command(pnd);
    }
    curl_global_cleanup();
    nfc_exit(context);
    exit(EXIT_FAILURE);
}

static void print_usage(const char *progname)
{
    printf("usage: %s mac\n", progname);
}



int main(int argc, const char *argv[])
{

    if (argc != 2)
    {
        print_usage(argv[0]);
        exit(EXIT_FAILURE   );
    }

    signal(SIGINT, stop_polling);
    squeeze_init("192.168.1.12", 9000, argv[1]);

    const nfc_modulation nmMifare = {
        .nmt = NMT_ISO14443A,
        .nbr = NBR_106,
    };
    nfc_target nt;
    int res = 0;

    nfc_init(&context);
    if (context == NULL) {
        printf("Unable to init libnfc (malloc)");
        exit(EXIT_FAILURE);
    }

    pnd = nfc_open(context, NULL);

    if (pnd == NULL)
    {
        printf("%s", "Unable to open NFC device.");
        nfc_exit(context);
        //exit(EXIT_FAILURE);
    }

    if (nfc_initiator_init(pnd) < 0)
    {
        nfc_perror(pnd, "nfc_initiator_init");
        nfc_close(pnd);
        nfc_exit(context);
        //exit(EXIT_FAILURE);
    }

    printf("NFC reader: %s opened\n", nfc_device_get_name(pnd));

    squeeze_reload_playlists();

    while(1)
    {
        if ((res = nfc_initiator_select_passive_target(pnd, nmMifare, NULL, 0, &nt)) < 0)
        {
            nfc_perror(pnd, "nfc_initiator_select_passive_target");
            continue;
        }

        if (res > 0)
        {
            char serial[32] = {0};
            for (int i = 0; i < nt.nti.nai.szUidLen; i++)
            {
                snprintf(serial+i*2, sizeof(serial), "%02x", nt.nti.nai.abtUid[i]);
            }

            printf("New serial detected : %s\n", serial);

            
            if (!strcmp(serial, PAUSE_ID))
            {
                squeeze_basic_action("pause");
            }
            else if (!strcmp(serial, NEXT_ID))
            {
                squeeze_basic_action("next");
            }
            else if (!strcmp(serial, PREV_ID))
            {
                squeeze_basic_action("prev");
            }
            else 
            {
                int id = squeeze_find_playlist_by_name(serial);
                if (id >= 0)
                {
                    squeeze_load_playlist(id);
                    /* Play the player when the card is inserted */
                    squeeze_basic_action("play");
                }
                else
                {
                    printf("%s not found in playlists : Rename a playlists like this : \"Playlist Name - %s\"\n", serial, serial);
                    // Reload playlists just in case
                    squeeze_reload_playlists();
                }
                if (_current)
                    free(_current);
                _current = strdup(serial);
            }
            while (0 == nfc_initiator_target_is_present(pnd, NULL))
            {
                
                // Do nothing
            }
            printf("Card removed, stop playing\n");

           
            /* Stop the player when the card is removed */
            //squeeze_basic_action("stop");
        }
        else
        {
            printf("No target found.\n");
        }
    }

    nfc_close(pnd);
    nfc_exit(context);
    exit(EXIT_SUCCESS);
}
