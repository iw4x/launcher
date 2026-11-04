// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string>
#include <vector>
#include <iostream>
#include <exception>

#include <ftxui/dom/node.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/screen.hpp>

#include <launcher/progress/progress-types.hxx>
#include <launcher/progress/progress-tracker.hxx>
#include <launcher/progress/progress-renderer.hxx>

using namespace std;
using namespace launcher;

namespace
{
  // Draw the row 120 columns wide, which fits it without shrinking, and
  // print it, followed by a line that marks the color of each cell (g for
  // green, l for light gray, d for dark gray, and a blank for the terminal's
  // color) and a line that marks the bold cells with '*'.
  //
  void
  print_row (ftxui::Element e)
  {
    using namespace ftxui;

    Screen sc (Screen::Create (Dimension::Fixed (120), Dimension::Fixed (1)));
    Render (sc, e);

    string t, c, b;
    for (int x (0); x != sc.dimx (); ++x)
    {
      const Pixel& p (sc.PixelAt (x, 0));
      const Color& f (p.foreground_color);

      t += p.character.empty () ? " " : p.character;
      c += f == Color::Green     ? 'g' :
           f == Color::GrayLight ? 'l' :
           f == Color::GrayDark  ? 'd' : ' ';
      b += p.bold ? '*' : ' ';
    }

    // Drop the trailing blanks so the expected output stays readable.
    //
    for (string* s: {&t, &c, &b})
      s->erase (s->find_last_not_of (' ') + 1);

    cout << t << '\n'
         << c << '\n'
         << b << endl;
  }

  progress_snapshot
  snapshot (const string& current, const string& total)
  {
    progress_snapshot s {};
    s.current_bytes = stoull (current);
    s.total_bytes = stoull (total);
    s.state = progress_state::active;
    return s;
  }
}

// Usage: argv[0] <command> <argument>...
//
// Format the arguments for the progress display and print the result. The
// commands are:
//
//    bytes <count>
//    speed <bytes-per-second>
//    duration <seconds>
//    bar <progress> <width> [<frame>]
//    indeterminate <width>
//    item <current-bytes> <total-bytes> <bar-width>
//    summary <current-bytes> <total-bytes>
//
// The progress is a ratio from 0 to 1. At no progress the bar shows the idle
// animation, with the frame selecting its step.
//
// The item and summary commands draw the row of the first of two files and
// the total row, respectively (see print_row() for the output).
//
int
main (int argc, char* argv[])
{
  vector<string> as (argv + 1, argv + argc);
  const string c (!as.empty () ? as[0] : "");
  const size_t n (as.size ());

  try
  {
    using pt = progress_tracker;

    if (c == "bytes" && n == 2)
      cout << pt::format_bytes (stoull (as[1])) << endl;
    else if (c == "speed" && n == 2)
      cout << pt::format_speed (stof (as[1])) << endl;
    else if (c == "duration" && n == 2)
      cout << pt::format_duration (stoi (as[1])) << endl;
    else if (c == "bar" && (n == 3 || n == 4))
      cout << pt::format_bar (stof (as[1]),
                              false,
                              stoi (as[2]),
                              n == 4 ? stoul (as[3]) : 0) << endl;
    else if (c == "indeterminate" && n == 2)
      cout << pt::format_bar (0, true, stoi (as[1])) << endl;
    else if (c == "item" && n == 4)
      print_row (progress_renderer::render_item (1,
                                                 2,
                                                 "file",
                                                 snapshot (as[1], as[2]),
                                                 stoi (as[3])));
    else if (c == "summary" && n == 3)
      print_row (progress_renderer::render_summary (0,
                                                    2,
                                                    snapshot (as[1], as[2])));
    else
    {
      cerr << "error: invalid arguments\n"
           << "  info: usage: driver <command> <argument>..." << endl;
      return 1;
    }
  }
  catch (const exception& e)
  {
    cerr << "error: " << e.what () << endl;
    return 1;
  }
}
