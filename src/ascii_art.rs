use crossterm::{
    execute,
    style::{Color, Print, SetForegroundColor},
};
use rand::Rng;

// shes a bit of a mess, but i like it
const ASCII_ART: [&str; 15] = [
    "    _____       ____ __      \n   /  _/ |     / / // / _  __\n   / / | | /| / / // /_| |/_/\n _/ /  | |/ |/ /__  __/>  <  \n/___/  |__/|__/  /_/ /_/|_|  \n",
    ",--.,--.   ,--.  ,---.            \n|  ||  |   |  | /    |,--.  ,--.  \n|  ||  |.'.|  |/  '  | \\  `'  /   \n|  ||   ,'.   |'--|  | /  /.  \\  \n`--''--'   '--'   `--''--'  '--'  \n",
    " _____        ___  _       \n|_ _\\ \\      / / || |__  __\n | | \\ \\ /\\ / /| || |\\ \\/ /\n | |  \\ V  V / |__   _>  < \n|___|  \\_/\\_/     |_|/_/\\_\\\n",
    " ________  __ __ __   __   __      __     __     \n/_______/\\/_//_//_/\\ /__/\\/__/\\   /__/\\ /__/\\    \n\\__.::._\\/\\:\\\\:\\\\:\\ \\\\  \\ \\: \\ \\__\\ \\::\\\\:.\\ \\   \n   \\::\\ \\  \\:\\\\:\\\\:\\ \\\\::\\_\\::\\/_/\\\\_\\::_\\:_\\/   \n   _\\::\\ \\__\\:\\\\:\\\\:\\ \\\\_:::   __\\/  _\\/__\\_\\_/\\ \n  /__\\::\\__/\\\\:\\\\:\\\\:\\ \\    \\::\\ \\   \\ \\ \\ \\::\\ \\\n  \\________\\/ \\_______\\/     \\__\\/    \\_\\/  \\__\\/",
    " _____  ____      ____  _    _            \n|_   _||_  _|    |_  _|| |  | |           \n  | |    \\ \\  /\\  / /  | |__| |_  _   __  \n  | |     \\ \\/  \\/ /   |____   _|[ \\ [  ] \n _| |_     \\  /\\  /        _| |_  > '  <  \n|_____|     \\/  \\/        |_____|[__]`\\_] ",
    "::::::::::: :::       :::     :::    :::    ::: \n    :+:     :+:       :+:    :+:     :+:    :+: \n    +:+     +:+       +:+   +:+ +:+   +:+  +:+  \n    +#+     +#+  +:+  +#+  +#+  +:+    +#++:+   \n    +#+     +#+ +#+#+ +#+ +#+#+#+#+#+ +#+  +#+  \n    #+#      #+#+# #+#+#        #+#  #+#    #+# \n###########   ###   ###         ###  ###    ### ",
    ">=> >=>        >=>                       \n>=> >=>        >=>      >=>              \n>=> >=>   >>   >=>     >>=>    >=>   >=> \n>=> >=>  >=>   >=>    > >=>      >> >=>  \n>=> >=> >> >=> >=>  >=> >=>       >>     \n>=> >> >>    >===> >===>>=>>=>  >>  >=>  \n>=> >=>        >=>      >=>    >=>   >=> ",
    "#### ##      ## ##        ##     ## \n ##  ##  ##  ## ##    ##   ##   ##  \n ##  ##  ##  ## ##    ##    ## ##   \n ##  ##  ##  ## ##    ##     ###    \n ##  ##  ##  ## #########   ## ##   \n ##  ##  ##  ##       ##   ##   ##  \n####  ###  ###        ##  ##     ## ",
    " _|_|_|  _|          _|  _|  _|              \n   _|    _|          _|  _|  _|    _|    _|  \n   _|    _|    _|    _|  _|_|_|_|    _|_|    \n   _|      _|  _|  _|        _|    _|    _|  \n _|_|_|      _|  _|          _|    _|    _|  ",
    "8888888 888       888     d8888           \n  888   888   o   888    d8P888           \n  888   888  d8b  888   d8P 888           \n  888   888 d888b 888  d8P  888  888  888 \n  888   888d88888b888 d88   888  `Y8bd8P' \n  888   88888P Y88888 8888888888   X88K   \n  888   8888P   Y8888       888  .d8\"\"8b. \n8888888 888P     Y888       888  888  888 ",
    "ooooo oooooo   oooooo     oooo       .o               \n`888'  `888.    `888.     .8'      .d88               \n 888    `888.   .8888.   .8'     .d'888   oooo    ooo \n 888     `888  .8'`888. .8'    .d'  888    `88b..8P'  \n 888      `888.8'  `888.8'     88ooo888oo    Y888'    \n 888       `888'    `888'           888    .o8\"'88b   \no888o       `8'      `8'           o888o  o88'   888o ",
    "@@@  @@@  @@@  @@@       @@@   @@@  @@@  \n@@@  @@@  @@@  @@@      @@@@   @@@  @@@  \n@@!  @@!  @@!  @@!     @@!@!   @@!  !@@  \n!@!  !@!  !@!  !@!    !@!!@!   !@!  @!!  \n!!@  @!!  !!@  @!@   @!! @!!    !@@!@!   \n!!!  !@!  !!!  !@!  !!!  !@!     @!!!    \n!!:  !!:  !!:  !!:  :!!:!:!!:   !: :!!   \n:!:  :!:  :!:  :!:  !:::!!:::  :!:  !:!  \n ::   :::: :: :::        :::    ::  :::  \n:      :: :  : :         :::    :   ::   ",
    "██╗░██╗░░░░░░░██╗░░██╗██╗██╗░░██╗\n██║░██║░░██╗░░██║░██╔╝██║╚██╗██╔╝\n██║░╚██╗████╗██╔╝██╔╝░██║░╚███╔╝░\n██║░░████╔═████║░███████║░██╔██╗░\n██║░░╚██╔╝░╚██╔╝░╚════██║██╔╝╚██╗\n╚═╝░░░╚═╝░░░╚═╝░░░░░░░╚═╝╚═╝░░╚═╝",
    "██████████████████████████\n█▄ ▄█▄ █▀▀▀█ ▄█ █ ██▄ ▀ ▄█\n██ ███ █ █ █ ██▄▄ ███▀ ▀██\n█▄▄▄██▄▄▄█▄▄▄███▄▄▄█▄▄█▄▄█\n▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀",
    " t                              ,W            \n Ej            ;               i##            \n E#,         .DL              f### :KW,      L\n E#t f.     :K#L     LWL     G####  ,#W:   ,KG\n E#t EW:   ;W##L   .E#f    .K#Ki##   ;#W. jWi \n E#t E#t  t#KE#L  ,W#;    ,W#D.,##    i#KED.  \n E#t E#t f#D.L#L t#K:    i##E,,i##,    L#W.   \n E#t E#jG#f  L#LL#G     ;DDDDDDE##DGi.GKj#K.  \n E#t E###;   L###j             ,##  iWf  i#K. \n E#t E#K:    L#W;              ,## LK:    t#E \n E#t EG      LE.               .E# i       tDj\n ,;. ;       ;@                  t            ",
];

pub fn print(colored: bool, index: usize) {
    if colored {
        print_colored(index);
    } else {
        println!("\n{}\n", ASCII_ART[index]);
    }
}

pub fn print_all(colored: bool) {
    for index in 0..ASCII_ART.len() {
        print(colored, index);
    }
}

pub fn print_random(colored: bool) {
    let random_index = rand::rng().random_range(0..ASCII_ART.len());
    print(colored, random_index);
}

pub fn print_colored(index: usize) {
    let mut rng = rand::rng();
    let base_hue = rng.random_range(0..360) as f32;

    println!();
    let mut char_count = 0;
    for ch in ASCII_ART[index].chars() {
        if ch == '\n' {
            // reset char count to get a continuous gradient across lines
            println!();
            char_count = 0;
        } else if ch != ' ' {
            let color = positional_color(base_hue, char_count);
            let _ = execute!(std::io::stdout(), SetForegroundColor(color), Print(ch));
            char_count += 1;
        } else {
            print!(" ");
            // theroetically we need to increase the count here,
            // but i think it looks cooler with the variation introduced by not increasing it
            // char_count += 1;
        }
    }

    let _ = execute!(std::io::stdout(), SetForegroundColor(Color::Reset));
    println!("\n");
}

fn positional_color(base_hue: f32, position: usize) -> Color {
    // shift hue by n degrees per char
    let hue_shift = (position as f32 * 12.0) % 360.0;
    let hue_final = (base_hue + hue_shift) % 360.0;

    // 0.35 saturation and 0.8 lightness because i say so
    let (r, g, b) = hsl_to_rgb(hue_final, 0.35, 0.8);
    Color::Rgb { r, g, b }
}

fn hsl_to_rgb(hue: f32, saturation: f32, lightness: f32) -> (u8, u8, u8) {
    // color segment 0-5, defines the color range
    let hue_segment = (hue / 60.0) % 6.0;
    // saturation of the primary color
    let chroma = (1.0 - (2.0 * lightness - 1.0).abs()) * saturation;
    // secondary color value
    let secondary = chroma * (1.0 - (hue_segment % 2.0 - 1.0).abs());
    // color lightness adjustment
    let adjustment = lightness - chroma / 2.0;

    // map hue segment to rgb values
    let (r, g, b) = match hue_segment as i32 {
        0 => (chroma, secondary, 0.0), // red to yellow
        1 => (secondary, chroma, 0.0), // yellow to green
        2 => (0.0, chroma, secondary), // green to cyan
        3 => (0.0, secondary, chroma), // cyan to blue
        4 => (secondary, 0.0, chroma), // blue to magenta
        _ => (chroma, 0.0, secondary), // magenta to red
    };

    // add lightness adjustment and convert to u8
    (
        ((r + adjustment) * 255.0) as u8,
        ((g + adjustment) * 255.0) as u8,
        ((b + adjustment) * 255.0) as u8,
    )
}
