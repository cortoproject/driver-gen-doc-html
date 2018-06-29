
#include "driver/gen/doc/html/html.h"
#include <corto/corto.h>
#include "corto/g/g.h"

typedef struct htmlData_t {
    g_generator g;
    corto_string output;
    corto_string title;
    corto_buffer content;
    corto_buffer index;
    corto_object docClass;
    corto_uint32 level;
} htmlData_t;

static char* doc_parentPath(char *buffer, int depth) {
    *buffer = '\0';
    while (depth--) {
        strcat(buffer, "../");
    }
    return buffer;
}

static void* html_getDocMember(corto_object doc, corto_string member, htmlData_t *data) {
    corto_member m = corto_interface_resolve_member(data->docClass, member);
    void *result = NULL;

    if (!corto_instanceof(data->docClass, doc)) {
        corto_critical("object '%s' is not of class '%s'",
            corto_fullpath(NULL, doc),
            corto_fullpath(NULL, data->docClass));
    }

    if (m) {
        result = CORTO_OFFSET(doc, m->offset);
    } else {
        corto_critical("member description not found in doc class");
    }

    return result;
}

static corto_string doc_parse(corto_string input) {
    corto_string result = NULL;

    corto_function f = corto_function(corto_resolve(NULL, "driver/ext/md/parse"));
    if (corto_check_state(f, CORTO_VALID)) {
        corto_invoke(f, &result, input);
    }
    return result;
}

static corto_string doc_getDescriptionFromDoc(corto_object doc, htmlData_t *data) {
    void *ptr = html_getDocMember(doc, "description", data);
    corto_string result = "";

    if (ptr && *(corto_string*)ptr) {
        result = *(corto_string*)ptr;
    }

    return doc_parse(result);
}

static corto_string doc_getNiceNameFromDoc(corto_object doc, htmlData_t *data) {
    void *ptr = html_getDocMember(doc, "niceName", data);
    corto_string result = "";

    if (ptr && *(corto_string*)ptr) {
        result = *(corto_string*)ptr;
    }

    return result;
}

static corto_int32 doc_getIndexFromDoc(corto_object doc, htmlData_t *data) {
    void *ptr = html_getDocMember(doc, "index", data);
    corto_int32 result = -1;

    if (ptr) {
        result = *(corto_int32*)ptr;
    }

    return result;
}


static corto_string doc_getTextFromDoc(corto_object doc, htmlData_t *data) {
    void *ptr = html_getDocMember(doc, "text", data);
    corto_string result = "";

    if (ptr && *(corto_string*)ptr) {
        result = *(corto_string*)ptr;
    }

    return result;
}

static int html_walkDocChilds(corto_object o, void *userData) {
    htmlData_t *data = userData;
    corto_string description = doc_getDescriptionFromDoc(o, data);
    corto_string text = doc_getTextFromDoc(o, data);
    corto_string niceName = doc_getNiceNameFromDoc(o, data);
    corto_object *docs = corto_alloc(sizeof(corto_object) * corto_scope_size(o));
    corto_id index;
    corto_object stack[CORTO_MAX_SCOPE_DEPTH];
    corto_object ptr = o;
    corto_int32 sp = data->level;
    corto_id link;
    corto_bool noIndex = TRUE;

    if (data->level < 4) {
        noIndex = FALSE;
    }

    corto_string parentId = corto_idof(corto_parentof(o));
    strcpy(link, parentId);
    strcat(link, "_");
    strcat(link, corto_idof(o));

    /* If level is 0, parse heading as title */
    if (!data->level) {
        data->title = niceName;
    } else {
        /* Add index number to header */
        index[0] = '\0';

        if (!noIndex) {
            do {
                stack[sp - 1] = ptr;
                ptr = corto_parentof(ptr);
            } while (--sp);

            for (; sp < data->level; sp ++) {
                char num[15];
                sprintf(num, "%d", doc_getIndexFromDoc(stack[sp], data));
                strcat(index, num);
                strcat(index, ".");
            }

            if (data->level > 2) {
                index[strlen(index) - 1] = '\0';
            }
            strcat(index, " ");
        }

        if (data->level == 1) {
            corto_buffer_append(&data->content,
                "<a id=\"%s\"></a>\n<p class=\"section\">%s</p>\n",
                link,
                /*index,*/
                niceName);
        } else {
            corto_buffer_append(&data->content,
                "<a id=\"%s\"></a>\n<h%d>%s</h%d>\n",
                link,
                data->level,
                /*index,*/
                niceName,
                data->level);
        }

        if (data->level == 2) {
            corto_buffer_append(&data->content, "<hr>\n");
        }

        if (data->level < 4) {
            corto_buffer_append(&data->index,
              "<li class=\"index-h%d\" data-level=\"%d\" data-header=\"%s\" onclick=\"navigate_index(this)\">",
                data->level,
                data->level,
                link,
                link);

            if (data->level == 1) {
                corto_buffer_appendstr(&data->index,
                    "<span class=\"material-icons\">subject</span>&nbsp;&nbsp;");
            } else if (data->level == 2) {
                corto_buffer_appendstr(&data->index,
                    "<span class=\"material-icons\">chevron_right</span>&nbsp;&nbsp;");
            } else if (data->level == 3) {
                corto_buffer_appendstr(&data->index,
                    "•&nbsp;&nbsp;");
            }

            if (data->level == 1) {
                corto_buffer_append(&data->index, "%s<span class=\"brief\">%s</span></li>\n",
                      niceName, description ? description : "");
            } else {
                corto_buffer_append(&data->index, "%s</li>\n",
                      niceName, description ? description : "");
            }
        }
    }

    corto_buffer_append(&data->content, "<div class=\"indent\">\n");

    corto_string parsedText = doc_parse(text);
    corto_buffer_append(&data->content, "%s\n", parsedText);
    corto_dealloc(parsedText);

    /* Collect documents in order */
    corto_objectseq seq = corto_scope_claim(o);
    corto_int32 i;
    for (i = 0; i < seq.length; i ++) {
        corto_object doc = seq.buffer[i];
        docs[doc_getIndexFromDoc(doc, data) - 1] = doc;
    }
    corto_scope_release(seq);

    /* Walk documents in order */
    data->level ++;

    for (i = 0; i < corto_scope_size(o); i ++) {
        if (!html_walkDocChilds(docs[i], userData)) {
            goto error;
        }
    }
    data->level --;

    corto_buffer_append(&data->content, "</div>\n");

    corto_dealloc(docs);

    return 1;
error:
    return 0;
}

corto_int16 genmain(g_generator g) {
    char *output = "html";

    if (corto_getenv("CORTO_DOCROOT")) {
        output = corto_getenv("CORTO_DOCROOT");
        corto_info("writing HTML to '%s'", output);
    }

    htmlData_t data = {
        g,
        output,
        NULL,
        CORTO_BUFFER_INIT,
        CORTO_BUFFER_INIT,
        corto_resolve(NULL, "driver/ext/md/Doc"),
        0
    };

    if (corto_mkdir(data.output)) {
        goto error;
    }

    if (!g_getCurrent(g)) {
        corto_object doc = corto_resolve(NULL, "doc");
        if (doc) {
            corto_id file;
            corto_string include;

            corto_scope_walk(doc, html_walkDocChilds, &data);

            if ((include = g_getAttribute(data.g, "include"))) {
                char *ext, *lastElem = strrchr(include, '/');
                if (lastElem) {
                    lastElem ++;
                } else {
                    lastElem = include;
                }
                sprintf(file, "%s/%s", data.output, lastElem);

                /* Quick & dirty way to replace extension .md with .html */
                ext = strchr(file, '.');
                if (ext) {
                    strcpy(ext, ".html");
                } else {
                    strcat(file, ".html");
                }
            } else {
                sprintf(file, "%s/doc.html", data.output);
            }

            corto_id template;
            char *templateFile;

            if (corto_file_test("template.html")) {
                templateFile = "template.html";
            } else {
                templateFile = g_getAttribute(g, "template");
            }
            if (!templateFile || !strlen(templateFile)) {
                sprintf(template, "%s/template.html", DRIVER_GEN_DOC_HTML_ETC);
                templateFile = template;
            }

            corto_string tmpl = corto_file_load(templateFile);
            if (tmpl) {
                corto_int32 depth = 0;
                char *namePtr = g_getName(g);
                if (!namePtr) {
                    corto_throw("no name specified");
                    goto error;
                }

                corto_id parentPath;
                while ((namePtr = strchr(namePtr, '/'))) {
                    depth ++;
                    namePtr ++;
                }
                doc_parentPath(parentPath, depth);
                corto_string content = corto_buffer_str(&data.content);
                corto_string index = corto_buffer_str(&data.index);
                corto_string t1 = strreplace(tmpl, "{{title}}", data.title);
                corto_string t2 = strreplace(t1, "{{content}}", content);
                t1 = strreplace(t2, "{{index}}", index);
                t1 = strreplace(t1, "{{parent-path}}", parentPath);
                FILE *f = fopen(file, "w");
                if (!f) {
                    corto_throw("failed to open '%s'", file);
                    goto error;
                }
                fprintf(f, "%s", t1);
                fclose(f);
                corto_dealloc(content);
                corto_dealloc(index);
            } else {
                corto_throw("template not found");
                goto error;
            }
        } else {
            corto_throw("no input for html generator");
            goto error;
        }
    }

    corto_release(data.docClass);

    return 0;
error:
    return -1;
}
