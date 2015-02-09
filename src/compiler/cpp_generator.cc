/*
 *
 * Copyright 2014, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <string>
#include <map>

#include "src/compiler/cpp_generator.h"

#include "src/compiler/cpp_generator_helpers.h"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

namespace grpc_cpp_generator {
namespace {

bool NoStreaming(const google::protobuf::MethodDescriptor *method) {
  return !method->client_streaming() && !method->server_streaming();
}

bool ClientOnlyStreaming(const google::protobuf::MethodDescriptor *method) {
  return method->client_streaming() && !method->server_streaming();
}

bool ServerOnlyStreaming(const google::protobuf::MethodDescriptor *method) {
  return !method->client_streaming() && method->server_streaming();
}

bool BidiStreaming(const google::protobuf::MethodDescriptor *method) {
  return method->client_streaming() && method->server_streaming();
}

bool HasUnaryCalls(const google::protobuf::FileDescriptor *file) {
  for (int i = 0; i < file->service_count(); i++) {
    for (int j = 0; j < file->service(i)->method_count(); j++) {
      if (NoStreaming(file->service(i)->method(j))) {
        return true;
      }
    }
  }
  return false;
}

bool HasClientOnlyStreaming(const google::protobuf::FileDescriptor *file) {
  for (int i = 0; i < file->service_count(); i++) {
    for (int j = 0; j < file->service(i)->method_count(); j++) {
      if (ClientOnlyStreaming(file->service(i)->method(j))) {
        return true;
      }
    }
  }
  return false;
}

bool HasServerOnlyStreaming(const google::protobuf::FileDescriptor *file) {
  for (int i = 0; i < file->service_count(); i++) {
    for (int j = 0; j < file->service(i)->method_count(); j++) {
      if (ServerOnlyStreaming(file->service(i)->method(j))) {
        return true;
      }
    }
  }
  return false;
}

bool HasBidiStreaming(const google::protobuf::FileDescriptor *file) {
  for (int i = 0; i < file->service_count(); i++) {
    for (int j = 0; j < file->service(i)->method_count(); j++) {
      if (BidiStreaming(file->service(i)->method(j))) {
        return true;
      }
    }
  }
  return false;
}
}  // namespace

std::string GetHeaderIncludes(const google::protobuf::FileDescriptor *file) {
  std::string temp =
      "#include <grpc++/impl/internal_stub.h>\n"
      "#include <grpc++/impl/service_type.h>\n"
      "#include <grpc++/status.h>\n"
      "\n"
      "namespace grpc {\n"
      "class ChannelInterface;\n"
      "class RpcService;\n"
      "class ServerContext;\n";
  if (HasUnaryCalls(file)) {
    temp.append(
        "template <class OutMessage> class ServerAsyncResponseWriter;\n");
  }
  if (HasClientOnlyStreaming(file)) {
    temp.append("template <class OutMessage> class ClientWriter;\n");
    temp.append("template <class InMessage> class ServerReader;\n");
    temp.append("template <class OutMessage> class ClientAsyncWriter;\n");
    temp.append("template <class InMessage> class ServerAsyncReader;\n");
  }
  if (HasServerOnlyStreaming(file)) {
    temp.append("template <class InMessage> class ClientReader;\n");
    temp.append("template <class OutMessage> class ServerWriter;\n");
    temp.append("template <class OutMessage> class ClientAsyncReader;\n");
    temp.append("template <class InMessage> class ServerAsyncWriter;\n");
  }
  if (HasBidiStreaming(file)) {
    temp.append(
        "template <class OutMessage, class InMessage>\n"
        "class ClientReaderWriter;\n");
    temp.append(
        "template <class OutMessage, class InMessage>\n"
        "class ServerReaderWriter;\n");
  }
  temp.append("}  // namespace grpc\n");
  return temp;
}

std::string GetSourceIncludes() {
  return "#include <grpc++/channel_interface.h>\n"
         "#include <grpc++/impl/rpc_method.h>\n"
         "#include <grpc++/impl/rpc_service_method.h>\n"
         "#include <grpc++/impl/service_type.h>\n"
         "#include <grpc++/stream.h>\n";
}

void PrintHeaderClientMethod(google::protobuf::io::Printer *printer,
                             const google::protobuf::MethodDescriptor *method,
                             std::map<std::string, std::string> *vars) {
  (*vars)["Method"] = method->name();
  (*vars)["Request"] =
      grpc_cpp_generator::ClassName(method->input_type(), true);
  (*vars)["Response"] =
      grpc_cpp_generator::ClassName(method->output_type(), true);
  if (NoStreaming(method)) {
    printer->Print(*vars,
                   "::grpc::Status $Method$(::grpc::ClientContext* context, "
                   "const $Request$& request, $Response$* response);\n");
    printer->Print(*vars,
                   "void $Method$(::grpc::ClientContext* context, "
                   "const $Request$& request, $Response$* response, "
                   "::grpc::Status *status, "
                   "::grpc::CompletionQueue *cq, void *tag);\n");
  } else if (ClientOnlyStreaming(method)) {
    printer->Print(*vars,
                   "::grpc::ClientWriter< $Request$>* $Method$("
                   "::grpc::ClientContext* context, $Response$* response);\n");
    printer->Print(*vars,
                   "::grpc::ClientWriter< $Request$>* $Method$("
                   "::grpc::ClientContext* context, $Response$* response, "
                   "::grpc::Status *status, "
                   "::grpc::CompletionQueue *cq, void *tag);\n");
  } else if (ServerOnlyStreaming(method)) {
    printer->Print(
        *vars,
        "::grpc::ClientReader< $Response$>* $Method$("
        "::grpc::ClientContext* context, const $Request$* request);\n");
    printer->Print(*vars,
                   "::grpc::ClientReader< $Response$>* $Method$("
                   "::grpc::ClientContext* context, const $Request$* request, "
                   "::grpc::CompletionQueue *cq, void *tag);\n");
  } else if (BidiStreaming(method)) {
    printer->Print(*vars,
                   "::grpc::ClientReaderWriter< $Request$, $Response$>* "
                   "$Method$(::grpc::ClientContext* context);\n");
    printer->Print(*vars,
                   "::grpc::ClientReaderWriter< $Request$, $Response$>* "
                   "$Method$(::grpc::ClientContext* context, "
                   "::grpc::CompletionQueue *cq, void *tag);\n");
  }
}

void PrintHeaderServerMethodSync(
    google::protobuf::io::Printer *printer,
    const google::protobuf::MethodDescriptor *method,
    std::map<std::string, std::string> *vars) {
  (*vars)["Method"] = method->name();
  (*vars)["Request"] =
      grpc_cpp_generator::ClassName(method->input_type(), true);
  (*vars)["Response"] =
      grpc_cpp_generator::ClassName(method->output_type(), true);
  if (NoStreaming(method)) {
    printer->Print(*vars,
                   "virtual ::grpc::Status $Method$("
                   "::grpc::ServerContext* context, const $Request$* request, "
                   "$Response$* response);\n");
  } else if (ClientOnlyStreaming(method)) {
    printer->Print(*vars,
                   "virtual ::grpc::Status $Method$("
                   "::grpc::ServerContext* context, "
                   "::grpc::ServerReader< $Request$>* reader, "
                   "$Response$* response);\n");
  } else if (ServerOnlyStreaming(method)) {
    printer->Print(*vars,
                   "virtual ::grpc::Status $Method$("
                   "::grpc::ServerContext* context, const $Request$* request, "
                   "::grpc::ServerWriter< $Response$>* writer);\n");
  } else if (BidiStreaming(method)) {
    printer->Print(
        *vars,
        "virtual ::grpc::Status $Method$("
        "::grpc::ServerContext* context, "
        "::grpc::ServerReaderWriter< $Response$, $Request$>* stream);"
        "\n");
  }
}

void PrintHeaderServerMethodAsync(
    google::protobuf::io::Printer *printer,
    const google::protobuf::MethodDescriptor *method,
    std::map<std::string, std::string> *vars) {
  (*vars)["Method"] = method->name();
  (*vars)["Request"] =
      grpc_cpp_generator::ClassName(method->input_type(), true);
  (*vars)["Response"] =
      grpc_cpp_generator::ClassName(method->output_type(), true);
  if (NoStreaming(method)) {
    printer->Print(*vars,
                   "void $Method$("
                   "::grpc::ServerContext* context, $Request$* request, "
                   "::grpc::ServerAsyncResponseWriter< $Response$>* response, "
                   "::grpc::CompletionQueue* cq, void *tag);\n");
  } else if (ClientOnlyStreaming(method)) {
    printer->Print(*vars,
                   "void $Method$("
                   "::grpc::ServerContext* context, "
                   "::grpc::ServerAsyncReader< $Request$>* reader, "
                   "$Response$* response, "
                   "::grpc::CompletionQueue* cq, void *tag);\n");
  } else if (ServerOnlyStreaming(method)) {
    printer->Print(*vars,
                   "void $Method$("
                   "::grpc::ServerContext* context, $Request$* request, "
                   "::grpc::ServerAsyncWriter< $Response$>* writer, "
                   "::grpc::CompletionQueue* cq, void *tag);\n");
  } else if (BidiStreaming(method)) {
    printer->Print(
        *vars,
        "void $Method$("
        "::grpc::ServerContext* context, "
        "::grpc::ServerReaderWriter< $Response$, $Request$>* stream, "
        "::grpc::CompletionQueue* cq, void *tag);\n");
  }
}

void PrintHeaderService(google::protobuf::io::Printer *printer,
                        const google::protobuf::ServiceDescriptor *service,
                        std::map<std::string, std::string> *vars) {
  (*vars)["Service"] = service->name();

  printer->Print(*vars,
                 "class $Service$ final {\n"
                 " public:\n");
  printer->Indent();

  // Client side
  printer->Print(
      "class Stub final : public ::grpc::InternalStub {\n"
      " public:\n");
  printer->Indent();
  for (int i = 0; i < service->method_count(); ++i) {
    PrintHeaderClientMethod(printer, service->method(i), vars);
  }
  printer->Outdent();
  printer->Print("};\n");
  printer->Print(
      "static Stub* NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& "
      "channel);\n");

  printer->Print("\n");

  // Server side - Synchronous
  printer->Print(
      "class Service : public ::grpc::SynchronousService {\n"
      " public:\n");
  printer->Indent();
  printer->Print("Service() : service_(nullptr) {}\n");
  printer->Print("virtual ~Service();\n");
  for (int i = 0; i < service->method_count(); ++i) {
    PrintHeaderServerMethodSync(printer, service->method(i), vars);
  }
  printer->Print("::grpc::RpcService* service() override final;\n");
  printer->Outdent();
  printer->Print(
      " private:\n"
      "  ::grpc::RpcService* service_;\n");
  printer->Print("};\n");

  // Server side - Asynchronous
  printer->Print(
      "class AsyncService final : public ::grpc::AsynchronousService {\n"
      " public:\n");
  printer->Indent();
  printer->Print("AsyncService() : service_(nullptr) {}\n");
  printer->Print("~AsyncService();\n");
  for (int i = 0; i < service->method_count(); ++i) {
    PrintHeaderServerMethodAsync(printer, service->method(i), vars);
  }
  printer->Print("::grpc::RpcService* service() override;\n");
  printer->Outdent();
  printer->Print(
      " private:\n"
      "  ::grpc::RpcService* service_;\n");
  printer->Print("};\n");

  printer->Outdent();
  printer->Print("};\n");
}

std::string GetHeaderServices(const google::protobuf::FileDescriptor *file) {
  std::string output;
  google::protobuf::io::StringOutputStream output_stream(&output);
  google::protobuf::io::Printer printer(&output_stream, '$');
  std::map<std::string, std::string> vars;

  for (int i = 0; i < file->service_count(); ++i) {
    PrintHeaderService(&printer, file->service(i), &vars);
    printer.Print("\n");
  }
  return output;
}

void PrintSourceClientMethod(google::protobuf::io::Printer *printer,
                             const google::protobuf::MethodDescriptor *method,
                             std::map<std::string, std::string> *vars) {
  (*vars)["Method"] = method->name();
  (*vars)["Request"] =
      grpc_cpp_generator::ClassName(method->input_type(), true);
  (*vars)["Response"] =
      grpc_cpp_generator::ClassName(method->output_type(), true);
  if (NoStreaming(method)) {
    printer->Print(*vars,
                   "::grpc::Status $Service$::Stub::$Method$("
                   "::grpc::ClientContext* context, "
                   "const $Request$& request, $Response$* response) {\n");
    printer->Print(*vars,
                   "return ::grpc::BlockingUnaryCall(channel(),"
                   "::grpc::RpcMethod(\"/$Package$$Service$/$Method$\"), "
                   "context, request, response);\n"
                   "}\n\n");
  } else if (ClientOnlyStreaming(method)) {
    printer->Print(
        *vars,
        "::grpc::ClientWriter< $Request$>* $Service$::Stub::$Method$("
        "::grpc::ClientContext* context, $Response$* response) {\n");
    printer->Print(*vars,
                   "  return new ::grpc::ClientWriter< $Request$>("
                   "channel(),"
                   "::grpc::RpcMethod(\"/$Package$$Service$/$Method$\", "
                   "::grpc::RpcMethod::RpcType::CLIENT_STREAMING), "
                   "context, response);\n"
                   "}\n\n");
  } else if (ServerOnlyStreaming(method)) {
    printer->Print(
        *vars,
        "::grpc::ClientReader< $Response$>* $Service$::Stub::$Method$("
        "::grpc::ClientContext* context, const $Request$* request) {\n");
    printer->Print(*vars,
                   "  return new ::grpc::ClientReader< $Response$>("
                   "channel(),"
                   "::grpc::RpcMethod(\"/$Package$$Service$/$Method$\", "
                   "::grpc::RpcMethod::RpcType::SERVER_STREAMING), "
                   "context, *request);\n"
                   "}\n\n");
  } else if (BidiStreaming(method)) {
    printer->Print(
        *vars,
        "::grpc::ClientReaderWriter< $Request$, $Response$>* "
        "$Service$::Stub::$Method$(::grpc::ClientContext* context) {\n");
    printer->Print(
        *vars,
        "  return new ::grpc::ClientReaderWriter< $Request$, $Response$>("
        "channel(),"
        "::grpc::RpcMethod(\"/$Package$$Service$/$Method$\", "
        "::grpc::RpcMethod::RpcType::BIDI_STREAMING), "
        "context);\n"
        "}\n\n");
  }
}

void PrintSourceServerMethod(google::protobuf::io::Printer *printer,
                             const google::protobuf::MethodDescriptor *method,
                             std::map<std::string, std::string> *vars) {
  (*vars)["Method"] = method->name();
  (*vars)["Request"] =
      grpc_cpp_generator::ClassName(method->input_type(), true);
  (*vars)["Response"] =
      grpc_cpp_generator::ClassName(method->output_type(), true);
  if (NoStreaming(method)) {
    printer->Print(*vars,
                   "::grpc::Status $Service$::Service::$Method$("
                   "::grpc::ServerContext* context, "
                   "const $Request$* request, $Response$* response) {\n");
    printer->Print(
        "  return ::grpc::Status("
        "::grpc::StatusCode::UNIMPLEMENTED);\n");
    printer->Print("}\n\n");
  } else if (ClientOnlyStreaming(method)) {
    printer->Print(*vars,
                   "::grpc::Status $Service$::Service::$Method$("
                   "::grpc::ServerContext* context, "
                   "::grpc::ServerReader< $Request$>* reader, "
                   "$Response$* response) {\n");
    printer->Print(
        "  return ::grpc::Status("
        "::grpc::StatusCode::UNIMPLEMENTED);\n");
    printer->Print("}\n\n");
  } else if (ServerOnlyStreaming(method)) {
    printer->Print(*vars,
                   "::grpc::Status $Service$::Service::$Method$("
                   "::grpc::ServerContext* context, "
                   "const $Request$* request, "
                   "::grpc::ServerWriter< $Response$>* writer) {\n");
    printer->Print(
        "  return ::grpc::Status("
        "::grpc::StatusCode::UNIMPLEMENTED);\n");
    printer->Print("}\n\n");
  } else if (BidiStreaming(method)) {
    printer->Print(*vars,
                   "::grpc::Status $Service$::Service::$Method$("
                   "::grpc::ServerContext* context, "
                   "::grpc::ServerReaderWriter< $Response$, $Request$>* "
                   "stream) {\n");
    printer->Print(
        "  return ::grpc::Status("
        "::grpc::StatusCode::UNIMPLEMENTED);\n");
    printer->Print("}\n\n");
  }
}

void PrintSourceService(google::protobuf::io::Printer *printer,
                        const google::protobuf::ServiceDescriptor *service,
                        std::map<std::string, std::string> *vars) {
  (*vars)["Service"] = service->name();
  printer->Print(
      *vars,
      "$Service$::Stub* $Service$::NewStub("
      "const std::shared_ptr< ::grpc::ChannelInterface>& channel) {\n"
      "  $Service$::Stub* stub = new $Service$::Stub();\n"
      "  stub->set_channel(channel);\n"
      "  return stub;\n"
      "};\n\n");
  for (int i = 0; i < service->method_count(); ++i) {
    PrintSourceClientMethod(printer, service->method(i), vars);
  }

  printer->Print(*vars,
                 "$Service$::Service::~Service() {\n"
                 "  delete service_;\n"
                 "}\n\n");
  for (int i = 0; i < service->method_count(); ++i) {
    PrintSourceServerMethod(printer, service->method(i), vars);
  }
  printer->Print(*vars,
                 "::grpc::RpcService* $Service$::Service::service() {\n");
  printer->Indent();
  printer->Print(
      "if (service_ != nullptr) {\n"
      "  return service_;\n"
      "}\n");
  printer->Print("service_ = new ::grpc::RpcService();\n");
  for (int i = 0; i < service->method_count(); ++i) {
    const google::protobuf::MethodDescriptor *method = service->method(i);
    (*vars)["Method"] = method->name();
    (*vars)["Request"] =
        grpc_cpp_generator::ClassName(method->input_type(), true);
    (*vars)["Response"] =
        grpc_cpp_generator::ClassName(method->output_type(), true);
    if (NoStreaming(method)) {
      printer->Print(
          *vars,
          "service_->AddMethod(new ::grpc::RpcServiceMethod(\n"
          "    \"/$Package$$Service$/$Method$\",\n"
          "    ::grpc::RpcMethod::NORMAL_RPC,\n"
          "    new ::grpc::RpcMethodHandler< $Service$::Service, $Request$, "
          "$Response$>(\n"
          "        std::function< ::grpc::Status($Service$::Service*, "
          "::grpc::ServerContext*, const $Request$*, $Response$*)>("
          "&$Service$::Service::$Method$), this),\n"
          "    new $Request$, new $Response$));\n");
    } else if (ClientOnlyStreaming(method)) {
      printer->Print(
          *vars,
          "service_->AddMethod(new ::grpc::RpcServiceMethod(\n"
          "    \"/$Package$$Service$/$Method$\",\n"
          "    ::grpc::RpcMethod::CLIENT_STREAMING,\n"
          "    new ::grpc::ClientStreamingHandler< "
          "$Service$::Service, $Request$, $Response$>(\n"
          "        std::function< ::grpc::Status($Service$::Service*, "
          "::grpc::ServerContext*, "
          "::grpc::ServerReader< $Request$>*, $Response$*)>("
          "&$Service$::Service::$Method$), this),\n"
          "    new $Request$, new $Response$));\n");
    } else if (ServerOnlyStreaming(method)) {
      printer->Print(
          *vars,
          "service_->AddMethod(new ::grpc::RpcServiceMethod(\n"
          "    \"/$Package$$Service$/$Method$\",\n"
          "    ::grpc::RpcMethod::SERVER_STREAMING,\n"
          "    new ::grpc::ServerStreamingHandler< "
          "$Service$::Service, $Request$, $Response$>(\n"
          "        std::function< ::grpc::Status($Service$::Service*, "
          "::grpc::ServerContext*, "
          "const $Request$*, ::grpc::ServerWriter< $Response$>*)>("
          "&$Service$::Service::$Method$), this),\n"
          "    new $Request$, new $Response$));\n");
    } else if (BidiStreaming(method)) {
      printer->Print(
          *vars,
          "service_->AddMethod(new ::grpc::RpcServiceMethod(\n"
          "    \"/$Package$$Service$/$Method$\",\n"
          "    ::grpc::RpcMethod::BIDI_STREAMING,\n"
          "    new ::grpc::BidiStreamingHandler< "
          "$Service$::Service, $Request$, $Response$>(\n"
          "        std::function< ::grpc::Status($Service$::Service*, "
          "::grpc::ServerContext*, "
          "::grpc::ServerReaderWriter< $Response$, $Request$>*)>("
          "&$Service$::Service::$Method$), this),\n"
          "    new $Request$, new $Response$));\n");
    }
  }
  printer->Print("return service_;\n");
  printer->Outdent();
  printer->Print("}\n\n");
}

std::string GetSourceServices(const google::protobuf::FileDescriptor *file) {
  std::string output;
  google::protobuf::io::StringOutputStream output_stream(&output);
  google::protobuf::io::Printer printer(&output_stream, '$');
  std::map<std::string, std::string> vars;
  // Package string is empty or ends with a dot. It is used to fully qualify
  // method names.
  vars["Package"] = file->package();
  if (!file->package().empty()) {
    vars["Package"].append(".");
  }

  for (int i = 0; i < file->service_count(); ++i) {
    PrintSourceService(&printer, file->service(i), &vars);
    printer.Print("\n");
  }
  return output;
}

}  // namespace grpc_cpp_generator
