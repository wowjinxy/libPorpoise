libdvd_srcs = files(
    'dvd.c',
    'dvderror.c',
    'dvdfs.c',
    'dvdlow.c',
    'dvdqueue.c',
	'fstload.c'
)

dvd_game_id_define = '-DLIBPORPOISE_DVD_GAME_ID="' + get_option('dvd_game_id') + '"'
dvd_company_define = '-DLIBPORPOISE_DVD_COMPANY="' + get_option('dvd_company') + '"'

libdvd = library('dvd',
    include_directories: libporpoise_includes,
    pic: false,
    sources: [libdvd_srcs],
    c_args: [
        dvd_game_id_define,
        dvd_company_define
    ],
    native: build_target != 'gc'
)

libporpoise_libs += libdvd
