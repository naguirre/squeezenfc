#pragma once

typedef struct _Playlist {
    const char *name;
    unsigned int id;
}Playlist;

void squeeze_next_song(void);
void squeeze_prev_song(void);
int squeeze_volume_get(void);
void squeeze_volume_set(int volume);
void squeeze_reload_playlists(void);
void squeeze_load_playlist(unsigned int playlist_id);
void squeeze_basic_action(const char *action);
int squeeze_find_playlist_by_name(const char *serial);
int squeeze_init(const char *host, int16_t port, const char *mac_addr);
