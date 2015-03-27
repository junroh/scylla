/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Copyright 2015 Cloudius Systems
 *
 * Modified by Cloudius Systems
 */

#pragma once

#include "cql3/restrictions/abstract_restriction.hh"
#include "cql3/restrictions/term_slice.hh"
#include "cql3/term.hh"
#include "core/shared_ptr.hh"
#include "schema.hh"
#include "to_string.hh"
#include "exceptions/exceptions.hh"

namespace cql3 {

namespace restrictions {

class single_column_restriction : public abstract_restriction {
protected:
    /**
     * The definition of the column to which apply the restriction.
     */
    const column_definition& _column_def;
public:
    single_column_restriction(const column_definition& column_def)
        : _column_def(column_def)
    { }

    const column_definition& get_column_def() {
        return _column_def;
    }
#if 0
    @Override
    public void addIndexExpressionTo(List<IndexExpression> expressions,
                                     QueryOptions options) throws InvalidRequestException
    {
        List<ByteBuffer> values = values(options);
        checkTrue(values.size() == 1, "IN restrictions are not supported on indexed columns");

        ByteBuffer value = validateIndexedValue(columnDef, values.get(0));
        expressions.add(new IndexExpression(columnDef.name.bytes, Operator.EQ, value));
    }

    @Override
    public boolean hasSupportingIndex(SecondaryIndexManager indexManager)
    {
        SecondaryIndex index = indexManager.getIndexForColumn(columnDef.name.bytes);
        return index != null && isSupportedBy(index);
    }

    /**
     * Check if this type of restriction is supported by the specified index.
     *
     * @param index the Secondary index
     * @return <code>true</code> this type of restriction is supported by the specified index,
     * <code>false</code> otherwise.
     */
    protected abstract boolean isSupportedBy(SecondaryIndex index);
#endif

    class EQ;

#if 0
    public static abstract class IN extends SingleColumnRestriction
    {
        public IN(ColumnDefinition columnDef)
        {
            super(columnDef);
        }

        @Override
        public final boolean isIN()
        {
            return true;
        }

        @Override
        public final Restriction mergeWith(Restriction otherRestriction) throws InvalidRequestException
        {
            throw invalidRequest("%s cannot be restricted by more than one relation if it includes a IN", columnDef.name);
        }

        @Override
        protected final boolean isSupportedBy(SecondaryIndex index)
        {
            return index.supportsOperator(Operator.IN);
        }
    }

    public static class InWithValues extends IN
    {
        protected final List<Term> values;

        public InWithValues(ColumnDefinition columnDef, List<Term> values)
        {
            super(columnDef);
            this.values = values;
        }

        @Override
        public boolean usesFunction(String ksName, String functionName)
        {
            return usesFunction(values, ksName, functionName);
        }

        @Override
        public List<ByteBuffer> values(QueryOptions options) throws InvalidRequestException
        {
            List<ByteBuffer> buffers = new ArrayList<>(values.size());
            for (Term value : values)
                buffers.add(value.bindAndGet(options));
            return buffers;
        }

        @Override
        public String toString()
        {
            return String.format("IN(%s)", values);
        }
    }

    public static class InWithMarker extends IN
    {
        protected final AbstractMarker marker;

        public InWithMarker(ColumnDefinition columnDef, AbstractMarker marker)
        {
            super(columnDef);
            this.marker = marker;
        }

        @Override
        public boolean usesFunction(String ksName, String functionName)
        {
            return false;
        }

        public List<ByteBuffer> values(QueryOptions options) throws InvalidRequestException
        {
            Term.MultiItemTerminal lval = (Term.MultiItemTerminal) marker.bind(options);
            if (lval == null)
                throw new InvalidRequestException("Invalid null value for IN restriction");
            return lval.getElements();
        }

        @Override
        public String toString()
        {
            return "IN ?";
        }
    }
#endif

    class slice;
    class contains;
};

class single_column_restriction::EQ final : public single_column_restriction {
private:
    ::shared_ptr<term> _value;
public:
    EQ(const column_definition& column_def, ::shared_ptr<term> value)
        : single_column_restriction(column_def)
        , _value(std::move(value))
    { }

    virtual bool uses_function(const sstring& ks_name, const sstring& function_name) override {
        return abstract_restriction::uses_function(_value, ks_name, function_name);
    }

    virtual bool is_EQ() override {
        return true;
    }

    virtual std::vector<bytes_opt> values(const query_options& options) override {
        std::vector<bytes_opt> v;
        v.push_back(_value->bind_and_get(options));
        return v;
    }

    virtual sstring to_string() override {
        return sprint("EQ(%s)", _value->to_string());
    }

    virtual void merge_with(::shared_ptr<restriction> other) {
        throw exceptions::invalid_request_exception(sprint(
            "%s cannot be restricted by more than one relation if it includes an Equal", _column_def.name_as_text()));
    }

#if 0
        @Override
        protected boolean isSupportedBy(SecondaryIndex index)
        {
            return index.supportsOperator(Operator.EQ);
        }
#endif
};

class single_column_restriction::slice : public single_column_restriction {
private:
    term_slice _slice;
public:
    slice(const column_definition& column_def, statements::bound bound, bool inclusive, ::shared_ptr<term> term)
        : single_column_restriction(column_def)
        , _slice(term_slice::new_instance(bound, inclusive, std::move(term)))
    { }

    virtual bool uses_function(const sstring& ks_name, const sstring& function_name) override {
        return (_slice.has_bound(statements::bound::START) && abstract_restriction::uses_function(_slice.bound(statements::bound::START), ks_name, function_name))
                || (_slice.has_bound(statements::bound::END) && abstract_restriction::uses_function(_slice.bound(statements::bound::END), ks_name, function_name));
    }

    virtual bool is_slice() override {
        return true;
    }

    virtual std::vector<bytes_opt> values(const query_options& options) override {
        throw exceptions::unsupported_operation_exception();
    }

    virtual bool has_bound(statements::bound b) override {
        return _slice.has_bound(b);
    }

    virtual std::vector<bytes_opt> bounds(statements::bound b, const query_options& options) override {
        return {_slice.bound(b)->bind_and_get(options)};
    }

    virtual bool is_inclusive(statements::bound b) override {
        return _slice.is_inclusive(b);
    }

    virtual void merge_with(::shared_ptr<restriction> r) override {
        if (!r->is_slice()) {
            throw exceptions::invalid_request_exception(sprint(
                "Column \"%s\" cannot be restricted by both an equality and an inequality relation", _column_def.name_as_text()));
        }

        auto other_slice = static_pointer_cast<slice>(r);

        if (has_bound(statements::bound::START) && other_slice->has_bound(statements::bound::START)) {
            throw exceptions::invalid_request_exception(sprint(
                   "More than one restriction was found for the start bound on %s", _column_def.name_as_text()));
        }

        if (has_bound(statements::bound::END) && other_slice->has_bound(statements::bound::END)) {
            throw exceptions::invalid_request_exception(sprint(
                "More than one restriction was found for the end bound on %s", _column_def.name_as_text()));
        }

        _slice.merge(other_slice->_slice);
    }

#if 0
    virtual void addIndexExpressionTo(List<IndexExpression> expressions, override
                                     QueryOptions options) throws InvalidRequestException
    {
        for (statements::bound b : {statements::bound::START, statements::bound::END})
        {
            if (has_bound(b))
            {
                ByteBuffer value = validateIndexedValue(columnDef, _slice.bound(b).bindAndGet(options));
                Operator op = _slice.getIndexOperator(b);
                // If the underlying comparator for name is reversed, we need to reverse the IndexOperator: user operation
                // always refer to the "forward" sorting even if the clustering order is reversed, but the 2ndary code does
                // use the underlying comparator as is.
                op = columnDef.isReversedType() ? op.reverse() : op;
                expressions.add(new IndexExpression(columnDef.name.bytes, op, value));
            }
        }
    }

    virtual bool isSupportedBy(SecondaryIndex index) override
    {
        return _slice.isSupportedBy(index);
    }
#endif

    virtual sstring to_string() override {
        return sprint("SLICE%s", _slice);
    }
};

// This holds CONTAINS, CONTAINS_KEY, and map[key] = value restrictions because we might want to have any combination of them.
class single_column_restriction::contains final : public single_column_restriction {
private:
    std::vector<::shared_ptr<term>> _values;
    std::vector<::shared_ptr<term>> _keys;
    std::vector<::shared_ptr<term>> _entry_keys;
    std::vector<::shared_ptr<term>> _entry_values;
public:
    contains(const column_definition& column_def, ::shared_ptr<term> t, bool is_key)
            : single_column_restriction(column_def) {
        if (is_key) {
            _keys.emplace_back(std::move(t));
        } else {
            _values.emplace_back(std::move(t));
        }
    }

    contains(const column_definition& column_def, ::shared_ptr<term> map_key, ::shared_ptr<term> map_value)
            : single_column_restriction(column_def) {
        _entry_keys.emplace_back(std::move(map_key));
        _entry_values.emplace_back(std::move(map_value));
    }

    virtual std::vector<bytes_opt> values(const query_options& options) override {
        return bind_and_get(_values, options);
    }

    virtual bool is_contains() override {
        return true;
    }

    virtual void merge_with(::shared_ptr<restriction> other_restriction) override {
        if (!other_restriction->is_contains()) {
            throw exceptions::invalid_request_exception(sprint(
                      "Collection column %s can only be restricted by CONTAINS, CONTAINS KEY, or map-entry equality",
                      get_column_def().name_as_text()));
        }

        auto other = static_pointer_cast<contains>(other_restriction);
        std::copy(other->_values.begin(), other->_values.end(), std::back_inserter(_values));
        std::copy(other->_keys.begin(), other->_keys.end(), std::back_inserter(_keys));
        std::copy(other->_entry_keys.begin(), other->_entry_keys.end(), std::back_inserter(_entry_keys));
        std::copy(other->_entry_values.begin(), other->_entry_values.end(), std::back_inserter(_entry_values));
    }

#if 0
        virtual void add_index_expression_to(std::vector<::shared_ptr<index_expression>>& expressions,
                const query_options& options) override {
            add_expressions_for(expressions, values(options), operator_type::CONTAINS);
            add_expressions_for(expressions, keys(options), operator_type::CONTAINS_KEY);
            add_expressions_for(expressions, entries(options), operator_type::EQ);
        }

        private void add_expressions_for(std::vector<::shared_ptr<index_expression>>& target, std::vector<bytes_opt> values,
                                       const operator_type& op) {
            for (ByteBuffer value : values)
            {
                validateIndexedValue(columnDef, value);
                target.add(new IndexExpression(columnDef.name.bytes, op, value));
            }
        }

        virtual bool is_supported_by(SecondaryIndex index) override {
            bool supported = false;

            if (numberOfValues() > 0)
                supported |= index.supportsOperator(Operator.CONTAINS);

            if (numberOfKeys() > 0)
                supported |= index.supportsOperator(Operator.CONTAINS_KEY);

            if (numberOfEntries() > 0)
                supported |= index.supportsOperator(Operator.EQ);

            return supported;
        }
#endif

    uint32_t number_of_values() {
        return _values.size();
    }

    uint32_t number_of_keys() {
        return _keys.size();
    }

    uint32_t number_of_entries() {
        return _entry_keys.size();
    }

    virtual bool uses_function(const sstring& ks_name, const sstring& function_name) override {
        return abstract_restriction::uses_function(_values, ks_name, function_name)
            || abstract_restriction::uses_function(_keys, ks_name, function_name)
            || abstract_restriction::uses_function(_entry_keys, ks_name, function_name)
            || abstract_restriction::uses_function(_entry_values, ks_name, function_name);
    }

    virtual sstring to_string() override {
        return sprint("CONTAINS(values=%s, keys=%s, entryKeys=%s, entryValues=%s)",
            ::to_string(_values), ::to_string(_keys), ::to_string(_entry_keys), ::to_string(_entry_values));
    }

    virtual bool has_bound(statements::bound b) override {
        throw exceptions::unsupported_operation_exception();
    }

    virtual std::vector<bytes_opt> bounds(statements::bound b, const query_options& options) override {
        throw exceptions::unsupported_operation_exception();
    }

    virtual bool is_inclusive(statements::bound b) override {
        throw exceptions::unsupported_operation_exception();
    }

#if 0
        private List<ByteBuffer> keys(const query_options& options) {
            return bindAndGet(keys, options);
        }

        private List<ByteBuffer> entries(QueryOptions options) throws InvalidRequestException
        {
            List<ByteBuffer> entryBuffers = new ArrayList<>(_entry_keys.size());
            List<ByteBuffer> keyBuffers = bindAndGet(_entry_keys, options);
            List<ByteBuffer> valueBuffers = bindAndGet(_entry_values, options);
            for (int i = 0; i < _entry_keys.size(); i++)
            {
                if (valueBuffers.get(i) == null)
                    throw new InvalidRequestException("Unsupported null value for map-entry equality");
                entryBuffers.add(CompositeType.build(keyBuffers.get(i), valueBuffers.get(i)));
            }
            return entryBuffers;
        }
#endif

private:
    /**
     * Binds the query options to the specified terms and returns the resulting values.
     *
     * @param terms the terms
     * @param options the query options
     * @return the value resulting from binding the query options to the specified terms
     * @throws invalid_request_exception if a problem occurs while binding the query options
     */
    static std::vector<bytes_opt> bind_and_get(std::vector<::shared_ptr<term>> terms, const query_options& options) {
        std::vector<bytes_opt> values;
        values.reserve(terms.size());
        for (auto&& term : terms) {
            values.emplace_back(term->bind_and_get(options));
        }
        return values;
    }
};


}

}
