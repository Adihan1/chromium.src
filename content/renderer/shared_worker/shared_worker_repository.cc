// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/shared_worker/shared_worker_repository.h"

#include "content/common/view_messages.h"
#include "content/renderer/render_frame_impl.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/web/WebSharedWorkerConnectListener.h"

#include "content/public/renderer/content_renderer_client.h"

namespace content {

SharedWorkerRepository::SharedWorkerRepository(
    service_manager::InterfaceProvider* interface_provider)
    : interface_provider_(interface_provider) {}

SharedWorkerRepository::~SharedWorkerRepository() = default;

void SharedWorkerRepository::Connect(
    bool isNodeJS,
    const blink::WebURL& url,
    const blink::WebString& name,
    DocumentID document_id,
    const blink::WebString& content_security_policy,
    blink::WebContentSecurityPolicyType content_security_policy_type,
    blink::WebAddressSpace creation_address_space,
    blink::mojom::SharedWorkerCreationContextType creation_context_type,
    bool data_saver_enabled,
    blink::MessagePortChannel channel,
    std::unique_ptr<blink::WebSharedWorkerConnectListener> listener) {
  // Lazy bind the connector.
  if (!connector_)
    interface_provider_->GetInterface(mojo::MakeRequest(&connector_));

  mojom::SharedWorkerInfoPtr info(mojom::SharedWorkerInfo::New(
      isNodeJS, GetContentClient()->renderer()->GetRootPath(),
      url, name.Utf8(), content_security_policy.Utf8(),
      content_security_policy_type, creation_address_space,
      data_saver_enabled));

  mojom::SharedWorkerClientPtr client;
  AddWorker(document_id,
            std::make_unique<SharedWorkerClientImpl>(std::move(listener)),
            mojo::MakeRequest(&client));

  connector_->Connect(std::move(info), std::move(client), creation_context_type,
                      channel.ReleaseHandle());
}

void SharedWorkerRepository::DocumentDetached(DocumentID document_id) {
  // Delete any associated SharedWorkerClientImpls, which will signal, via the
  // dropped mojo connection, disinterest in the associated shared worker.
  client_map_.erase(document_id);
}

void SharedWorkerRepository::AddWorker(
    DocumentID document_id,
    std::unique_ptr<mojom::SharedWorkerClient> impl,
    mojom::SharedWorkerClientRequest request) {
  std::pair<ClientMap::iterator, bool> result =
      client_map_.emplace(document_id, nullptr);
  std::unique_ptr<ClientSet>& clients = result.first->second;
  if (!clients)
    clients = std::make_unique<ClientSet>();
  clients->AddBinding(std::move(impl), std::move(request));
}

}  // namespace content
