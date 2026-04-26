#pragma once

#include <launcher/progress/progress-types.hxx>
#include <launcher/progress/progress-tracker.hxx>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <vector>
#include <string>
#include <functional>
#include <atomic>
#include <thread>

namespace launcher
{
  struct progress_item
  {
    std::string label;
    progress_snapshot snapshot;

    progress_item () = default;

    progress_item (std::string lbl, progress_snapshot snap)
      : label (std::move (lbl)), snapshot (std::move (snap))
    {
    }
  };

  struct progress_render_context
  {
    std::vector<progress_item> items;
    progress_snapshot overall;
    std::vector<std::string> log_messages;
    std::size_t completed_count {0};
    std::size_t total_count {0};
    unsigned frame {0};

    bool dialog_visible {false};
    std::string dialog_title;
    std::string dialog_message;

    progress_render_context () = default;
  };

  class progress_renderer
  {
  public:
    static constexpr int default_bar_width = 15;
    static constexpr std::size_t max_log_messages = 5;

    static ftxui::Element render_item (const std::string& label,
                                       const progress_snapshot& snapshot,
                                       int bar_width = default_bar_width,
                                       unsigned frame = 0);

    static ftxui::Element render_summary (std::size_t completed,
                                          std::size_t total,
                                          const progress_snapshot& overall,
                                          unsigned frame = 0);

    static ftxui::Element render_logs (const std::vector<std::string>& messages);

    static ftxui::Element render_status (const std::string& status);

    progress_renderer ();
    ~progress_renderer ();

    progress_renderer (const progress_renderer&) = delete;
    progress_renderer& operator= (const progress_renderer&) = delete;

    progress_renderer (progress_renderer&&) = delete;
    progress_renderer& operator= (progress_renderer&&) = delete;

    void start ();
    void stop ();
    void update (progress_render_context&& ctx) noexcept;
    void refresh () noexcept;

    bool
    running () const noexcept
    {
      return running_.load (std::memory_order_relaxed);
    }

  private:
    ftxui::Component create_component ();

    progress_render_context contexts_[2];
    std::atomic<int> render_buffer_ {0};

    ftxui::ScreenInteractive screen_;
    ftxui::Component component_;

    std::atomic<bool> running_ {false};
    std::thread ui_thread_;
  };
}
