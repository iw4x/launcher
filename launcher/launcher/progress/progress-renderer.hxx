#pragma once

#include <launcher/progress/progress-types.hxx>
#include <launcher/progress/progress-tracker.hxx>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace launcher
{
  // Renderer traits for customization.
  //
  template <typename S = std::string>
  struct progress_renderer_traits
  {
    using string_type = S;

    // Default bar width for rendering.
    //
    static constexpr int default_bar_width = 15;

    // Maximum number of log messages to display.
    //
    static constexpr std::size_t max_log_messages = 5;

    // Render a single progress item.
    //
    static ftxui::Element
    render_item (const string_type& label,
                 const progress_snapshot& snapshot,
                 int bar_width = default_bar_width);

    // Render a summary line.
    //
    static ftxui::Element
    render_summary (std::size_t completed,
                    std::size_t total,
                    const progress_snapshot& overall);

    // Render log messages.
    //
    static ftxui::Element
    render_logs (const std::vector<string_type>& messages);

    // Render status message.
    //
    static ftxui::Element
    render_status (const string_type& status);
  };

  // Item to render (lock-free snapshot).
  //
  template <typename S = std::string>
  struct basic_progress_item
  {
    using string_type = S;

    string_type label;
    progress_snapshot snapshot;

    basic_progress_item () = default;

    basic_progress_item (string_type lbl, progress_snapshot snap)
      : label (std::move (lbl)), snapshot (std::move (snap))
    {
    }
  };

  // Rendering context (double-buffered, lock-free read).
  //
  template <typename S = std::string>
  struct basic_progress_render_context
  {
    using string_type = S;
    using item_type = basic_progress_item<string_type>;

    std::vector<item_type> items;
    progress_snapshot overall;
    std::vector<string_type> log_messages;
    std::size_t completed_count {0};
    std::size_t total_count {0};

    // Dialog state (optional modal overlay).
    //
    bool dialog_visible {false};
    string_type dialog_title;
    string_type dialog_message;

    basic_progress_render_context () = default;
  };

  // FTXUI-based progress renderer (lock-free, async-safe).
  //
  template <typename T = progress_renderer_traits<>>
  class basic_progress_renderer
  {
  public:
    using traits_type = T;
    using string_type = typename traits_type::string_type;
    using item_type = basic_progress_item<string_type>;
    using context_type = basic_progress_render_context<string_type>;

    basic_progress_renderer ();
    ~basic_progress_renderer ();

    // Non-copyable, non-movable (manages screen).
    //
    basic_progress_renderer (const basic_progress_renderer&) = delete;
    basic_progress_renderer& operator= (const basic_progress_renderer&) = delete;

    basic_progress_renderer (basic_progress_renderer&&) = delete;
    basic_progress_renderer& operator= (basic_progress_renderer&&) = delete;

    // Start rendering (non-blocking).
    //
    void
    start ();

    // Stop rendering.
    //
    void
    stop ();

    // Update render context (lock-free write to inactive buffer).
    //
    void
    update (context_type&& ctx) noexcept;

    // Trigger a refresh.
    //
    void
    refresh () noexcept;

    // Check if running.
    //
    bool
    running () const noexcept
    {
      return running_.load (std::memory_order_relaxed);
    }

  private:
    // Create FTXUI component for rendering.
    //
    ftxui::Component
    create_component ();

    // Double-buffered context (lock-free).
    //
    context_type contexts_[2];
    std::atomic<int> render_buffer_ {0};  // Active buffer index

    ftxui::ScreenInteractive screen_;
    ftxui::Component component_;

    std::atomic<bool> running_ {false};
    std::jthread ui_thread_;
  };

  using progress_item = basic_progress_item<>;
  using progress_render_context = basic_progress_render_context<>;
  using progress_renderer = basic_progress_renderer<>;
}

#include <launcher/progress/progress-renderer.txx>
