#include <launcher/shortcut/shortcut-writer.hxx>

#include <cstddef>
#include <string>
#include <system_error>
#include <vector>

#include <windows.h>

#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>
#include <shlobj.h>

using namespace std;

namespace launcher
{
  namespace
  {
    void
    check (HRESULT r, const char* w)
    {
      if (FAILED (r))
        throw system_error (static_cast<int> (r), system_category (), w);
    }

    class com_guard
    {
    public:
      com_guard ()
      {
        HRESULT r (
          CoInitializeEx (nullptr,
                          COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));

        if (r == RPC_E_CHANGED_MODE)
          return;

        check (r, "failed to initialize COM");

        owned_ = true;
      }

      ~com_guard ()
      {
        if (owned_)
          CoUninitialize ();
      }

      com_guard (const com_guard&) = delete;
      com_guard& operator = (const com_guard&) = delete;

    private:
      bool owned_ = false;
    };

    template <typename I>
    class com_ptr
    {
    public:
      com_ptr () = default;
     ~com_ptr ()
      {
        if (p_ != nullptr)
          p_->Release ();
      }

      com_ptr (const com_ptr&) = delete;
      com_ptr& operator = (const com_ptr&) = delete;

      I*
      operator ->() const noexcept
      {
        return p_;
      }

      explicit
      operator bool () const noexcept
      {
        return p_ != nullptr;
      }

      void**
      operator & () noexcept
      {
        return reinterpret_cast<void**> (&p_);
      }

    private:
      I* p_ = nullptr;
    };

    optional<fs::path>
    known_folder (REFKNOWNFOLDERID i)
    {
      PWSTR b (nullptr);

      HRESULT r (SHGetKnownFolderPath (i, KF_FLAG_DEFAULT, nullptr, &b));

      if (b == nullptr)
        return nullopt;

      optional<fs::path> p;

      if (SUCCEEDED (r))
        p = fs::path (b);

      CoTaskMemFree (b);

      return p;
    }

    wstring
    widen (const string& s)
    {
      if (s.empty ())
        return wstring ();

      int n (static_cast<int> (s.size ()));
      int w (MultiByteToWideChar (CP_UTF8, 0, s.data (), n, nullptr, 0));

      if (w <= 0)
        return wstring ();

      wstring r (static_cast<size_t> (w), L'\0');

      MultiByteToWideChar (CP_UTF8, 0, s.data (), n, r.data (), w);

      return r;
    }

    wstring
    command_line (const vector<string>& as)
    {
      wstring r;

      for (const string& a : as)
      {
        if (!r.empty ())
          r += L' ';

        wstring w (widen (a));

        if (!w.empty () && w.find_first_of (L" \t\n\v\\\"") == wstring::npos)
        {
          r += w;
          continue;
        }

        r += L'"';

        for (size_t i (0); i != w.size (); ++i)
        {
          size_t b (0);

          while (i != w.size () && w[i] == L'\\')
          {
            ++i;
            ++b;
          }

          if (i == w.size ())
          {
            r.append (b * 2, L'\\');
            break;
          }

          if (w[i] == L'"')
          {
            r.append (b * 2 + 1, L'\\');
            r += L'"';
          }
          else
          {
            r.append (b, L'\\');
            r += w[i];
          }
        }

        r += L'"';
      }

      return r;
    }

    bool
    matches (const fs::path& p,
             const wstring& tg,
             const wstring& ar,
             const wstring& wd,
             const wstring& ds,
             const wstring& ic)
    {
      com_ptr<IShellLinkW> l;

      if (FAILED (CoCreateInstance (CLSID_ShellLink,
                                    nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_IShellLinkW,
                                    &l)))
        return false;

      com_ptr<IPersistFile> f;

      if (FAILED (l->QueryInterface (IID_IPersistFile, &f)))
        return false;

      if (FAILED (f->Load (p.wstring ().c_str (), STGM_READ)))
        return false;

      wchar_t sb[MAX_PATH];

      if (FAILED (l->GetPath (sb, MAX_PATH, nullptr, SLGP_RAWPATH)) || tg != sb)
        return false;

      wchar_t lb[INFOTIPSIZE];

      if (FAILED (l->GetArguments (lb, INFOTIPSIZE)) || ar != lb)
        return false;

      if (FAILED (l->GetWorkingDirectory (sb, MAX_PATH)) || wd != sb)
        return false;

      if (FAILED (l->GetDescription (lb, INFOTIPSIZE)) || ds != lb)
        return false;

      int n (0);

      if (FAILED (l->GetIconLocation (sb, MAX_PATH, &n)) || ic != sb)
        return false;

      return true;
    }
  }

  bool shortcut_writer::
  supported (shortcut_scope) noexcept
  {
    return true;
  }

  optional<fs::path> shortcut_writer::
  directory (shortcut_scope s)
  {
    switch (s)
    {
      case shortcut_scope::desktop:
        return known_folder (FOLDERID_Desktop);

      case shortcut_scope::menu:
      {
        if (optional<fs::path> d = known_folder (FOLDERID_Programs))
          return *d / "IW4x";

        return nullopt;
      }
    }

    return nullopt;
  }

  optional<fs::path> shortcut_writer::
  location (const shortcut_spec& c, shortcut_scope s)
  {
    if (optional<fs::path> d = directory (s))
      return *d / (c.name + ".lnk");

    return nullopt;
  }

  bool shortcut_writer::
  write (const shortcut_spec& c, shortcut_scope s)
  {
    com_guard com;

    optional<fs::path> p (location (c, s));

    if (!p)
      return false;

    wstring tg (c.target.wstring ());
    wstring ar (command_line (c.arguments));
    wstring wd (c.working_directory.wstring ());
    wstring ds (widen (c.comment));

    error_code ec;
    wstring ic;

    if (!c.icon.empty () && fs::exists (c.icon, ec))
      ic = c.icon.wstring ();

    if (matches (*p, tg, ar, wd, ds, ic))
      return false;

    ec.clear ();

    fs::create_directories (p->parent_path (), ec);

    if (ec)
      throw system_error (ec,
                          "failed to create shortcut directory: " +
                            p->parent_path ().string ());

    com_ptr<IShellLinkW> l;

    check (CoCreateInstance (CLSID_ShellLink,
                             nullptr,
                             CLSCTX_INPROC_SERVER,
                             IID_IShellLinkW,
                             &l),
           "failed to create shell link object");

    check (l->SetPath (tg.c_str ()), "failed to set shortcut target");
    check (l->SetArguments (ar.c_str ()), "failed to set shortcut arguments");
    check (l->SetWorkingDirectory (wd.c_str ()),
           "failed to set shortcut working directory");
    check (l->SetDescription (ds.c_str ()),
           "failed to set shortcut description");

    if (!ic.empty ())
      check (l->SetIconLocation (ic.c_str (), 0),
             "failed to set shortcut icon");

    com_ptr<IPersistFile> f;

    check (l->QueryInterface (IID_IPersistFile, &f),
           "failed to query shortcut persistence interface");

    check (f->Save (p->wstring ().c_str (), TRUE), "failed to save shortcut");

    return true;
  }
}
