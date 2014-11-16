/*
Copyright (c) 2003-2006, Vrije Universiteit
All rights reserved.

Redistribution and use of the GLOBULE system in source and binary forms,
with or without modification, are permitted provided that the following
conditions are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above
     copyright notice, this list of conditions and the following
     disclaimer in the documentation and/or other materials provided
     with the distribution.

   * Neither the name of Vrije Universiteit nor the names of the software
     authors or contributors may be used to endorse or promote
     products derived from this software without specific prior
     written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS, AUTHORS, AND
CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL VRIJE UNIVERSITEIT OR ANY AUTHORS OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This product includes software developed by the Apache Software Foundation
<http://www.apache.org/>.
*/
#ifndef _CONFIGURATION_HPP
#define _CONFIGURATION_HPP

class HandlerConfiguration
{
private:
  int         _redirect_mode;
  const char* _redirect_policy;
  const char* _replication_policy;
  int         _reevaluateinterval;
  int         _reevaluateadaptinterval;
  int         _reevaluatecontentinterval;
  apr_uint16_t _weight;
  std::vector<Contact>* _contacts;
public:
  inline HandlerConfiguration(int redirect_mode,
			      const char *redirect_policy,
			      const char *replication_policy,
			      int reevaluateinterval,
			      int reevaluateadaptinterval,
			      int reevaluatecontentinterval,
			      apr_uint16_t myweight,
			      std::vector<Contact>* contacts)
    throw()
    : _redirect_mode(redirect_mode), _redirect_policy(redirect_policy),
      _replication_policy(replication_policy),
      _reevaluateinterval(reevaluateinterval),
      _reevaluateadaptinterval(reevaluateadaptinterval),
      _reevaluatecontentinterval(reevaluatecontentinterval),
      _weight(myweight),
      _contacts(contacts)
  {
  };
  inline int get_redirect_mode() const throw() {
    return _redirect_mode;
  };
  inline const char *get_redirect_policy() const throw() {
    return _redirect_policy;
  };
  inline const char *get_replication_policy() const throw() {
    return _replication_policy;
  };
  inline const int get_reevaluateinterval() const throw() {
    return _reevaluateinterval;
  };
  inline const apr_uint16_t get_weight() const throw() {
    return _weight;
  };
  const int get_reevaluateadaptskip() const throw();
  const int get_reevaluatecontentskip() const throw();
  inline const std::vector<Contact>* get_contacts() const throw() {
    return _contacts;
  };
};

#endif
