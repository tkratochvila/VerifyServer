#include "subprocess.hpp"

namespace subprocess {

void Popen::init_args() {
  populate_c_argv();
}

/// Start the child process. Only to be used when
/// `defer_spawn` option was provided in Popen constructor.
  void Popen::start_process() //throw (CalledProcessError, OSError)
{
  // The process was started/tried to be started
  // in the constructor itself.
  // For explicitly calling this API to start the
  // process, 'defer_spawn' argument must be set to
  // true in the constructor.
  if (!defer_process_start_)
    return;
  execute_process();
}

void Popen::populate_c_argv()
{
  cargv_.clear();
  cargv_.reserve(vargs_.size() + 1);
  for (auto& arg : vargs_) cargv_.push_back(&arg[0]);
  cargv_.push_back(nullptr);
}

/// Wait for the child to exit.
  int Popen::wait() //throw (OSError)
{
  int ret, status;
  std::tie(ret, status) = util::wait_for_child_exit(pid());
  if (ret == -1) {
    if (errno != ECHILD) throw OSError("waitpid failed", errno);
    return 0;
  }
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  if (WIFSIGNALED(status)) return WTERMSIG(status);
  else return 255;

  return 0;
}

/// Check the status of the running child.
/// -2 returned if child is still running.
  int Popen::poll() //throw (OSError)
{
  int status;
  if (!child_created_) return -1; // TODO: ??

  // Returns zero if child is still running. If successful, waitpid returns the process ID of the terminated process
  // whose status was reported. If unsuccessful, a -1 is returned.
  int ret = waitpid(child_pid_, &status, WNOHANG);
  if (ret == 0) return -2;

  if (ret == child_pid_) {
    if (WIFSIGNALED(status)) {
      retcode_ = WTERMSIG(status);
    } else if (WIFEXITED(status)) {	// returns true if the child terminated normally, that is,
        // by calling exit(3) or _exit(2), or by returning from main().
      retcode_ = WEXITSTATUS(status);   //  returns the exit status of the child. This consists
         // of the least significant 8 bits of the status argument that the child specified
         // in a call to exit(3) or _exit(2) or as the argument for a return statement in main().
    } else {
      retcode_ = 255;
    }
    return retcode_;
  }

  if (ret == -1) {
    // From subprocess.py
    // This happens if SIGCHLD is set to be ignored
    // or waiting for child process has otherwise been disabled
    // for our process. This child is dead, we cannot get the
    // status.
    if (errno == ECHILD) retcode_ = 0;
    else throw OSError("waitpid failed", errno);
  } else {
    retcode_ = ret;
  }

  return retcode_;
}

/// Kill the child. SIGTERM used by default.
void Popen::kill(int sig_num)
{
  if (session_leader_) killpg(child_pid_, sig_num);
  else ::kill(child_pid_, sig_num);
}


  void Popen::execute_process() //throw (CalledProcessError, OSError)
  {
    int err_rd_pipe, err_wr_pipe;
    std::tie(err_rd_pipe, err_wr_pipe) = util::pipe_cloexec();
    std::cout << "Executing: " << util::join(vargs_) << std::endl;
    if (shell_) {
      auto new_cmd = util::join(vargs_);
      vargs_.clear();
      vargs_.insert(vargs_.begin(), {"/bin/sh", "-c"});
      vargs_.push_back(new_cmd);
      populate_c_argv();
    }

    if (exe_name_.length()) {
      vargs_.insert(vargs_.begin(), exe_name_);
      populate_c_argv();
    }
    exe_name_ = vargs_[0];

    child_pid_ = fork();

    if (child_pid_ < 0) {
      close(err_rd_pipe);
      close(err_wr_pipe);
      throw OSError("fork failed", errno);
    }

    child_created_ = true;

    if (child_pid_ == 0)
    {
      // Close descriptors belonging to parent
      stream_.close_parent_fds();

      //Close the read end of the error pipe
      close(err_rd_pipe);

      detail::Child chld(this, err_wr_pipe);
      chld.execute_child();
    } 
    else 
    {
      //int sys_ret = -1;
      close (err_wr_pipe);// close child side of pipe, else get stuck in read below

      stream_.close_child_fds();

      try {
        char err_buf[SP_MAX_ERR_BUF_SIZ] = {0,};

        int read_bytes = util::read_atmost_n(
				  err_rd_pipe, 
				  err_buf, 
				  SP_MAX_ERR_BUF_SIZ);
        close(err_rd_pipe);

        if (read_bytes || strlen(err_buf)) {
          // Call waitpid to reap the child process
          // waitpid suspends the calling process until the
          // child terminates.
          wait();

          // Throw whatever information we have about child failure
          throw CalledProcessError(err_buf);
        }
      } catch (std::exception& exp) {
        stream_.cleanup_fds();
        throw exp;
      }

    }
  }

namespace detail {

  void ArgumentDeducer::set_option(executable&& exe) {
    popen_->exe_name_ = std::move(exe.arg_value);
  }

  void ArgumentDeducer::set_option(cwd&& cwdir) {
    popen_->cwd_ = std::move(cwdir.arg_value);
  }

  void ArgumentDeducer::set_option(bufsize&& bsiz) {
    popen_->stream_.bufsiz_ = bsiz.bufsiz;
  }

  void ArgumentDeducer::set_option(environment&& env) {
    popen_->env_ = std::move(env.env_);
  }

  void ArgumentDeducer::set_option(defer_spawn&& defer) {
    popen_->defer_process_start_ = defer.defer;
  }

  void ArgumentDeducer::set_option(shell&& sh) {
    popen_->shell_ = sh.shell_;
  }

  void ArgumentDeducer::set_option(session_leader&& sleader) {
    popen_->session_leader_ = sleader.leader_;
  }

  void ArgumentDeducer::set_option(input&& inp) {
    if (inp.rd_ch_ != -1) popen_->stream_.read_from_parent_ = inp.rd_ch_;
    if (inp.wr_ch_ != -1) popen_->stream_.write_to_child_ = inp.wr_ch_;
  }

  void ArgumentDeducer::set_option(output&& out) {
    if (out.wr_ch_ != -1) popen_->stream_.write_to_parent_ = out.wr_ch_;
    if (out.rd_ch_ != -1) popen_->stream_.read_from_child_ = out.rd_ch_;
  }

  void ArgumentDeducer::set_option(error&& err) {
    if (err.deferred_) {
      if (popen_->stream_.write_to_parent_) {
        popen_->stream_.err_write_ = popen_->stream_.write_to_parent_;
      } else {
        throw std::runtime_error("Set output before redirecting error to output");
      }
    }
    if (err.wr_ch_ != -1) popen_->stream_.err_write_ = err.wr_ch_;
    if (err.rd_ch_ != -1) popen_->stream_.err_read_ = err.rd_ch_;
  }

  void ArgumentDeducer::set_option(close_fds&& cfds) {
    popen_->close_fds_ = cfds.close_all;
  }

  void ArgumentDeducer::set_option(preexec_func&& prefunc) {
    popen_->preexec_fn_ = std::move(prefunc);
    popen_->has_preexec_fn_ = true;
  }


  void Child::execute_child() {
    int sys_ret = -1;
    auto& stream = parent_->stream_;

    try {
      if (stream.write_to_parent_ == 0)
      	stream.write_to_parent_ = dup(stream.write_to_parent_);

      if (stream.err_write_ == 0 || stream.err_write_ == 1)
      	stream.err_write_ = dup(stream.err_write_);

      // Make the child owned descriptors as the
      // stdin, stdout and stderr for the child process
      auto _dup2_ = [](int fd, int to_fd) {
        if (fd == to_fd) {
          // dup2 syscall does not reset the
          // CLOEXEC flag if the descriptors
          // provided to it are same.
          // But, we need to reset the CLOEXEC
          // flag as the provided descriptors
          // are now going to be the standard
          // input, output and error
          util::set_clo_on_exec(fd, false);
        } else if(fd != -1) {
          int res = dup2(fd, to_fd);
          if (res == -1) throw OSError("dup2 failed", errno);
        }
      };

      // Create the standard streams
      _dup2_(stream.read_from_parent_, 0); // Input stream
      _dup2_(stream.write_to_parent_,  1); // Output stream
      _dup2_(stream.err_write_,        2); // Error stream

      // Close the duped descriptors
      if (stream.read_from_parent_ != -1 && stream.read_from_parent_ > 2) 
      	close(stream.read_from_parent_);

      if (stream.write_to_parent_ != -1 && stream.write_to_parent_ > 2) 
      	close(stream.write_to_parent_);

      if (stream.err_write_ != -1 && stream.err_write_ > 2) 
      	close(stream.err_write_);

      // Close all the inherited fd's except the error write pipe
      if (parent_->close_fds_) {
        int max_fd = sysconf(_SC_OPEN_MAX);
        if (max_fd == -1) throw OSError("sysconf failed", errno);

        for (int i = 3; i < max_fd; i++) {
          if (i == err_wr_pipe_) continue;
          close(i);
        }
      }

      // Change the working directory if provided
      if (parent_->cwd_.length()) {
        sys_ret = chdir(parent_->cwd_.c_str());
        if (sys_ret == -1) throw OSError("chdir failed", errno);
      }

      if (parent_->has_preexec_fn_) {
      	parent_->preexec_fn_();
      }

      if (parent_->session_leader_) {
      	sys_ret = setsid();
      	if (sys_ret == -1) throw OSError("setsid failed", errno);
      }

      // Replace the current image with the executable
      if (parent_->env_.size()) {
        for (auto& kv : parent_->env_) {
          setenv(kv.first.c_str(), kv.second.c_str(), 1);
	}
        sys_ret = execvp(parent_->exe_name_.c_str(), parent_->cargv_.data());
      } else {
        sys_ret = execvp(parent_->exe_name_.c_str(), parent_->cargv_.data());
      }

      if (sys_ret == -1) throw OSError("execve failed", errno);

    } catch (const OSError& exp) {
      // Just write the exception message
      // TODO: Give back stack trace ?
      std::string err_msg(exp.what());
      //ATTN: Can we do something on error here ?
      util::write_n(err_wr_pipe_, err_msg.c_str(), err_msg.length());
      throw exp;
    }

    // Calling application would not get this
    // exit failure
    exit (EXIT_FAILURE);
  }


  void Streams::setup_comm_channels()
  {
    if (write_to_child_ != -1)  input(fdopen(write_to_child_, "wb"));
    if (read_from_child_ != -1) output(fdopen(read_from_child_, "rb"));
    if (err_read_ != -1)        error(fdopen(err_read_, "rb"));

    auto handles = {input(), output(), error()};

    for (auto& h : handles) {
      if (h == nullptr) continue;
      switch (bufsiz_) {
      case 0:
	setvbuf(h, nullptr, _IONBF, BUFSIZ);
	break;
      case 1:
	setvbuf(h, nullptr, _IONBF, BUFSIZ);
	break;
      default:
	setvbuf(h, nullptr, _IOFBF, bufsiz_);
      };
    }
  }

  int Communication::send(const char* msg, size_t length)
  {
    if (stream_->input() == nullptr) return -1;
    return std::fwrite(msg, sizeof(char), length, stream_->input());
  }

  int Communication::send(const std::vector<char>& msg)
  {
    return send(msg.data(), msg.size());
  }

  std::pair<OutBuffer, ErrBuffer>
  Communication::communicate(const char* msg, size_t length)
  {
    // Optimization from subprocess.py
    // If we are using one pipe, or no pipe
    // at all, using select() or threads is unnecessary.
    auto hndls = {stream_->input(), stream_->output(), stream_->error()};
    int count = std::count(std::begin(hndls), std::end(hndls), nullptr);

    if (count >= 2) {
      OutBuffer obuf;
      ErrBuffer ebuf;
      if (stream_->input()) {
      	if (msg) {
	  size_t wbytes = std::fwrite(msg, sizeof(char), length, stream_->input());
	  if (wbytes < length) {
	    if (errno != EPIPE && errno != EINVAL) {
	      throw OSError("fwrite error", errno);
	    }
	  }
	}
	// Close the input stream
	stream_->input_.reset();
      } else if (stream_->output()) {
      	// Read till EOF
      	// ATTN: This could be blocking, if the process
      	// at the other end screws up, we get screwed up as well
      	obuf.add_cap(out_buf_cap_);

	int rbytes = util::read_all(
				  fileno(stream_->output()),
				  obuf.buf);

	if (rbytes == -1) {
	  throw OSError("read to obuf failed", errno);
	}

	obuf.length = rbytes;
	// Close the output stream
	stream_->output_.reset();

      } else if (stream_->error()) {
      	// Same screwness applies here as well
      	ebuf.add_cap(err_buf_cap_);

      	int rbytes = util::read_atmost_n(
				  fileno(stream_->error()),
				  ebuf.buf.data(), 
				  ebuf.buf.size());

      	if (rbytes == -1) {
      	  throw OSError("read to ebuf failed", errno);
	}

	ebuf.length = rbytes;
	// Close the error stream
	stream_->error_.reset();
      }
      return std::make_pair(std::move(obuf), std::move(ebuf));
    }

    return communicate_threaded(msg, length);
  }


  std::pair<OutBuffer, ErrBuffer>
  Communication::communicate_threaded(const char* msg, size_t length)
  {
    OutBuffer obuf;
    ErrBuffer ebuf;
    std::future<int> out_fut, err_fut;

    if (stream_->output()) {
      obuf.add_cap(out_buf_cap_);

      out_fut = std::async(std::launch::async,
                          [&obuf, this] {
                            return util::read_all(fileno(this->stream_->output()), obuf.buf);
                          });
    }
    if (stream_->error()) {
      ebuf.add_cap(err_buf_cap_);

      err_fut = std::async(std::launch::async,
                          [&ebuf, this] {
                            return util::read_all(fileno(this->stream_->error()), ebuf.buf);
                          });
    }
    if (stream_->input()) {
      if (msg) {
	size_t wbytes = std::fwrite(msg, sizeof(char), length, stream_->input());
	if (wbytes < length) {
	  if (errno != EPIPE && errno != EINVAL) {
	    throw OSError("fwrite error", errno);
	  }
	}
      }
      stream_->input_.reset();
    }

    if (out_fut.valid()) {
      int res = out_fut.get();
      if (res != -1) obuf.length = res;
      else obuf.length = 0;
    }
    if (err_fut.valid()) {
      int res = err_fut.get();
      if (res != -1) ebuf.length = res;
      else ebuf.length = 0;
    }

    return std::make_pair(std::move(obuf), std::move(ebuf));
  }

} // end namespace detail

}
