namespace hello
{
  template <typename T>
  inline void basic_http_session<T>::
  configure_ssl ()
  {
    // If the certificate file is specified, use that. Otherwise fall back to
    // the system default verify paths.
    //
    if (!traits_.ssl_cert_file.empty ())
      ssl_ctx_.load_verify_file (traits_.ssl_cert_file);
    else
      ssl_ctx_.set_default_verify_paths ();

    // Verify the peer unless the user explicitly asked us not to.
    //
    if (traits_.verify_ssl)
      ssl_ctx_.set_verify_mode (ssl::verify_peer);
    else
      ssl_ctx_.set_verify_mode (ssl::verify_none);

    // Disable the old, insecure protocols (SSLv2/v3) and enable the
    // implementation workarounds.
    //
    // Also enable single DH use to have a fresh key for each handshake.
    //
    ssl_ctx_.set_options (ssl::context::default_workarounds |
                          ssl::context::no_sslv2 |
                          ssl::context::no_sslv3 |
                          ssl::context::single_dh_use);

    // Clear the server name callback to make sure it doesn't interfere with the
    // SNI handling which is done by the stream.
    //
    SSL_CTX_set_tlsext_servername_callback (ssl_ctx_.native_handle (), nullptr);
  }

  template <typename T>
  inline asio::awaitable<typename basic_http_client<T>::response_type>
  basic_http_client<T>::
  get (const string_type& url)
  {
    request_type req (http_method::get, url);
    req.normalize ();
    co_return co_await request (req);
  }

  template <typename T>
  inline asio::awaitable<typename basic_http_client<T>::response_type>
  basic_http_client<T>::
  post (const string_type& url,
        const string_type& body,
        const string_type& content_type)
  {
    request_type req (http_method::post, url);
    req.set_content_type (content_type);
    req.set_body (body);
    req.normalize ();
    co_return co_await request (req);
  }

  template <typename T>
  inline asio::awaitable<typename basic_http_client<T>::response_type>
  basic_http_client<T>::
  put (const string_type& url,
       const string_type& body,
       const string_type& content_type)
  {
    request_type req (http_method::put, url);
    req.set_content_type (content_type);
    req.set_body (body);
    req.normalize ();
    co_return co_await request (req);
  }

  template <typename T>
  inline asio::awaitable<typename basic_http_client<T>::response_type>
  basic_http_client<T>::
  delete_ (const string_type& url)
  {
    request_type req (http_method::delete_, url);
    req.normalize ();
    co_return co_await request (req);
  }

  template <typename T>
  inline asio::awaitable<typename basic_http_client<T>::response_type>
  basic_http_client<T>::
  head (const string_type& url)
  {
    request_type req (http_method::head, url);
    req.normalize ();
    co_return co_await request (req);
  }

  template <typename T>
  inline asio::awaitable<typename basic_http_client<T>::response_type>
  basic_http_client<T>::
  request (const request_type& req)
  {
    co_return co_await request_impl (req, 0);
  }
}
