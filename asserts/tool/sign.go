// -*- Mode: Go; indent-tabs-mode: t -*-

/*
 * Copyright (C) 2016 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// Package tool offers tooling to sign assertions.
package tool

import (
	"fmt"

	"gopkg.in/yaml.v2"

	"github.com/snapcore/snapd/asserts"
)

// SignRequest specifies the complete input for signing an assertion.
type SignRequest struct {
	// KeyID specifies the key id of the key to use
	KeyID string

	// Statement is used as input to construct the assertion
	// it's a mapping encoded as YAML
	// of the header fields of the assertion
	// plus an optional pseudo-header "body" to specify
	// the body of the assertion
	Statement []byte
}

// Sign produces the text of a signed assertion as specified by req.
func Sign(req *SignRequest, keypairMgr asserts.KeypairManager) ([]byte, error) {
	var headers map[string]interface{}
	err := yaml.Unmarshal(req.Statement, &headers)
	if err != nil {
		return nil, fmt.Errorf("cannot parse the assertion input as YAML: %v", err)
	}
	typCand, ok := headers["type"]
	if !ok {
		return nil, fmt.Errorf("missing assertion type header")
	}
	typStr, ok := typCand.(string)
	if !ok {
		return nil, fmt.Errorf("assertion type must be a string, not: %v", typCand)
	}
	typ := asserts.Type(typStr)
	if typ == nil {
		return nil, fmt.Errorf("invalid assertion type: %v", headers["type"])
	}

	var body []byte
	if bodyCand, ok := headers["body"]; ok {
		bodyStr, ok := bodyCand.(string)
		if !ok {
			return nil, fmt.Errorf("body if specified must be a string")
		}
		body = []byte(bodyStr)
		delete(headers, "body")
	}

	keyID := req.KeyID

	adb, err := asserts.OpenDatabase(&asserts.DatabaseConfig{
		KeypairManager: keypairMgr,
	})
	if err != nil {
		return nil, err
	}

	// TODO: teach Sign to cross check keyID and authority-id
	// against an account-key
	a, err := adb.Sign(typ, headers, body, keyID)
	if err != nil {
		return nil, err
	}

	return asserts.Encode(a), nil
}
