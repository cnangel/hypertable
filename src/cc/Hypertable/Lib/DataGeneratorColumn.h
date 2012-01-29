/** -*- c++ -*-
 * Copyright (C) 2007-2012 Hypertable, Inc.
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 3 of the
 * License, or any later version.
 *
 * Hypertable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef HYPERTABLE_DATAGENERATORCOLUMN_H
#define HYPERTABLE_DATAGENERATORCOLUMN_H

#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

extern "C" {
#include <limits.h>
#include <stdlib.h>
}

#include "Common/Config.h"
#include "Common/FileUtils.h"
#include "Common/Random.h"
#include "Common/String.h"

#include "Cell.h"
#include "DataGeneratorRowComponent.h"
#include "DataGeneratorQualifier.h"

using namespace Hypertable::Config;
using namespace std;

namespace Hypertable {

  class ColumnSpec {
  public:
    ColumnSpec() : size(-1), to_stdout(false) { }
    QualifierSpec qualifier;
    int size;
    int order;
    String source;
    String column_family;
    unsigned seed;
    String distribution;
    bool to_stdout;
  };

  class Column : public ColumnSpec {
  public:
    Column(ColumnSpec &spec) : ColumnSpec(spec) {
      if (spec.qualifier.type != -1)
        m_qualifiers.push_back( QualifierFactory::create(spec.qualifier) );
      m_next_qualifier = m_qualifiers.size();
    }
    virtual ~Column() { }
    virtual bool next() = 0;
    virtual String &qualifier() = 0;
    virtual const char *value() = 0;
    virtual uint32_t value_len() = 0;
  protected:
    std::vector<Qualifier *> m_qualifiers;
    size_t m_next_qualifier;
  };

  class ColumnString : public Column {
  public:
    ColumnString(ColumnSpec &spec, bool keys_only=false) : Column(spec), m_keys_only(keys_only) {
      if (source == "") {
        m_value_data_len = size * 50;
        m_value_data.reset( new char [ m_value_data_len ] );
        Random::fill_buffer_with_random_ascii((char *)m_value_data.get(), m_value_data_len);
	m_source = (const char *)m_value_data.get();
      }
      else {
	m_source = (const char *)FileUtils::mmap(source, &m_value_data_len);
        HT_ASSERT(m_value_data_len >= size);
      }
      m_value_data_len -= size;
      m_render_buf.reset( new char [ size * 2 ] + 1 );
    }
    virtual ~ColumnString() { }
    virtual bool next() {
      if (m_qualifiers.empty())
        m_next_qualifier = 0;
      else
        m_next_qualifier = (m_next_qualifier + 1) % m_qualifiers.size();
      if (m_next_qualifier == 0 && !m_keys_only) {
        off_t offset = Random::number32() % m_value_data_len;
	if (to_stdout) {
	  m_value = m_source + offset;
	}
	else {
	  const char *src = m_source + offset;
	  char *dst = m_render_buf.get();
	  for (size_t i=0; i<(size_t)size; i++) {
	    if (*src == '\n') {
	      *dst++ = '\\';
	      *dst++ = 'n';
	    }
	    else if (*src == '\t') {
	      *dst++ = '\\';
	      *dst++ = 't';
	    }
	    else if (*src == '\0') {
	      *dst++ = '\\';
	      *dst++ = '0';
	    }
	    else
	      *dst++ = *src;
	    src++;
	  }
	  *dst = 0;
	  m_value = m_render_buf.get();
	}
      }
      if (m_qualifiers.empty())
        return false;
      m_qualifiers[m_next_qualifier]->next();
      if (m_next_qualifier == (m_qualifiers.size()-1))
        return false;
      return true;
    }
    virtual String &qualifier() {
      if (m_qualifiers.empty())
        return m_qualifier;
      return m_qualifiers[m_next_qualifier]->get();
    }
    virtual const char *value() {
      return m_value;
    }
    virtual uint32_t value_len() {
      return size;
    }
  private:
    bool m_keys_only;
    const char *m_value;
    String m_qualifier;
    boost::shared_array<char> m_render_buf;
    boost::shared_array<const char> m_value_data;
    const char *m_source;
    off_t m_value_data_len;
  };

}

#endif // HYPERTABLE_DATAGENERATORCOLUMN_H
