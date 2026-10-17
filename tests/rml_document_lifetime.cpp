// Real pinned RmlUi core; headless render/font interfaces. No fake object
// layout, vtable overwrite, copied document implementation or guessed caller.
#include <RmlUi/Core.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::abort();}} while(0)
struct Renderer : Rml::RenderInterface {
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>,Rml::Span<const int>) override {return 1;}
    void RenderGeometry(Rml::CompiledGeometryHandle,Rml::Vector2f,Rml::TextureHandle) override {}
    void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
    Rml::TextureHandle LoadTexture(Rml::Vector2i&,const Rml::String&) override {return 0;}
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>,Rml::Vector2i) override {return 0;}
    void ReleaseTexture(Rml::TextureHandle) override {}
    void EnableScissorRegion(bool) override {}
    void SetScissorRegion(Rml::Rectanglei) override {}
};
// A pointer to an inherited protected member has type Element::* and can be
// applied to real Element objects. This exposes the existing query for tests;
// no downcast to a fake Access object or production header access change.
struct Access : Rml::Element {
    static auto query() {return &Access::IsLayoutDirty;}
    static auto dirty() {return &Access::DirtyLayout;}
};
static bool dirty(Rml::Element* e) {return (e->*Access::query())();}
struct Audit : Rml::Plugin {
    bool queryDuringDestruction = false;
    unsigned documents = 0;
    int GetEventClasses() override {return EVT_ELEMENT;}
    void OnElementDestroy(Rml::Element* e) override {
        if(!e->HasAttribute("lifetime-test")) return;
        ++documents;
        if(queryDuringDestruction) {
            // A controlled invocation at the exact destructor notification
            // boundary. It reproduces the mechanism, not Odin's unknown caller.
            std::fprintf(stderr,"destruction-query\n");std::fflush(stderr);
            CHECK(!dirty(e));
            CHECK(!e->GetOwnerDocument());
        }
    }
};
struct RecursiveDocument : Rml::ElementDocument {
    RecursiveDocument() : Rml::ElementDocument("body") {}
    bool IsLayoutDirty() override {return Element::IsLayoutDirty();}
};
static Rml::ElementDocument* makeDocument(Rml::Context* context) {
    auto* doc=context->LoadDocumentFromMemory(
        "<rml><head><style>body{width:400px;height:300px;}"
        "div{display:block;width:100px;height:30px;}input{width:80px;height:20px;}"
        "</style></head><body><div id='panel'><input type='button' id='button'/></div></body></rml>");
    CHECK(doc && doc->GetOwnerDocument()==doc);
    doc->SetAttribute("lifetime-test",Rml::String("1"));
    CHECK(doc->GetElementById("button")->GetOwnerDocument()==doc);
    doc->Show();context->Update();context->Update();
    CHECK(!dirty(doc));
    auto* panel=doc->GetElementById("panel");CHECK(panel->GetBox().GetSize().x==100.f);
    (panel->*Access::dirty())();CHECK(dirty(panel) && dirty(doc));
    context->Update();CHECK(!dirty(panel) && !dirty(doc));
    panel->SetProperty("width","120px");context->Update();CHECK(panel->GetBox().GetSize().x==120.f);
    doc->GetElementById("button")->Focus();context->Update();context->Render();
    // Include the non-DOM path in the owned child vector as well.
    doc->AppendChild(doc->CreateElement("div"),false);context->Update();
    CHECK(doc->GetNumChildren(true)>doc->GetNumChildren());
    return doc;
}
int main(int argc,char** argv) {
    Renderer renderer;Rml::FontEngineInterface font;Rml::SetRenderInterface(&renderer);Rml::SetFontEngineInterface(&font);
    CHECK(Rml::Initialise());
    if(argc>1 && !std::strcmp(argv[1],"--cycle")) {
        RecursiveDocument doc;(void)dirty(&doc);CHECK(false);
    }
    Audit audit;audit.queryDuringDestruction=argc>1 && !std::strcmp(argv[1],"--destroy-query");
    Rml::RegisterPlugin(&audit);
    auto* context=Rml::CreateContext("lifetime",{800,600});CHECK(context);
    for(unsigned i=0;i<12;++i) {
        auto* menu=makeDocument(context);auto* overlay=makeDocument(context);
        menu->Hide();overlay->Hide();menu->Show();context->Update();
        // Detached subtrees must retain no owner, and may outlive the document.
        auto detached=menu->RemoveChild(menu->GetElementById("panel"));CHECK(!detached->GetOwnerDocument());
        CHECK(!detached->GetChild(0)->GetOwnerDocument());
        if(i%3==0) {menu->Close();overlay->Close();}
        else if(i%3==1) {context->UnloadDocument(menu);context->UnloadDocument(overlay);}
        else context->UnloadAllDocuments();
        context->Update();context->Update();CHECK(context->GetNumDocuments()==0);
        CHECK(!dirty(detached.get()));
    }
    // Context teardown also owns live documents, not just previously closed ones.
    makeDocument(context);makeDocument(context);CHECK(Rml::RemoveContext("lifetime"));
    CHECK(audit.documents==26);
    Rml::UnregisterPlugin(&audit);Rml::Shutdown();
    std::puts("PASS: real RmlUi layout/dirty flags, menu/overlay transitions, focus, DOM/non-DOM ownership, retained detached trees, close/unload/context teardown");
}
