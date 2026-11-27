#include "mechanics/Meshing.h"

#include <cmath>

namespace resonant::mechanics {

namespace {
constexpr double pi = 3.14159265358979323846;
}

StructuralModel frameLine(const Vec3& start, const Vec3& end, int elements, const std::function<Section(double)>& sectionAt,
                          const ElasticMaterial& material, const Vec3& up)
{
    StructuralModel model;
    model.kind = ElementKind::Frame;
    model.materials = {material};
    elements = std::max(1, elements);
    for (int i = 0; i <= elements; ++i)
        model.nodes.push_back(start + (end - start) * (static_cast<double>(i) / elements));
    for (int i = 0; i < elements; ++i) {
        model.sections.push_back(sectionAt((i + 0.5) / elements));
        Element e;
        e.nodes[0] = i;
        e.nodes[1] = i + 1;
        e.section = i;
        e.up = up;
        model.elements.push_back(e);
    }
    model.fixed.assign(model.nodes.size(), 0);
    return model;
}

StructuralModel frameLine(const Vec3& start, const Vec3& end, int elements, const Section& section, const ElasticMaterial& material,
                          const Vec3& up)
{
    StructuralModel model = frameLine(start, end, elements, [&](double) { return section; }, material, up);
    // One section shared by all elements.
    model.sections = {section};
    for (Element& e : model.elements)
        e.section = 0;
    return model;
}

StructuralModel rectangularGrid(ElementKind kind, const Vec3& centre, const Vec3& axis1, const Vec3& axis2, double a, double b, int n1,
                                int n2, double thickness, double tension, const ElasticMaterial& material)
{
    StructuralModel model;
    model.kind = kind;
    model.materials = {material};
    model.axis1 = axis1.normalized();
    model.axis2 = axis2.normalized();
    model.normal = model.axis1.cross(model.axis2).normalized();
    n1 = std::max(1, n1);
    n2 = std::max(1, n2);
    for (int j = 0; j <= n2; ++j)
        for (int i = 0; i <= n1; ++i)
            model.nodes.push_back(centre + (a * (static_cast<double>(i) / n1 - 0.5)) * model.axis1
                                  + (b * (static_cast<double>(j) / n2 - 0.5)) * model.axis2);
    for (int j = 0; j < n2; ++j)
        for (int i = 0; i < n1; ++i) {
            Element e;
            const int n0 = i + (n1 + 1) * j;
            e.nodes[0] = n0;
            e.nodes[1] = n0 + 1;
            e.nodes[2] = n0 + 1 + (n1 + 1);
            e.nodes[3] = n0 + (n1 + 1);
            e.thickness = thickness;
            e.tension = tension;
            model.elements.push_back(e);
        }
    model.fixed.assign(model.nodes.size(), 0);
    return model;
}

StructuralModel hexBox(const Vec3& centre, const Vec3& axis1, const Vec3& axis2, const Vec3& axis3, const Vec3& size, int n1, int n2,
                       int n3, const ElasticMaterial& material)
{
    StructuralModel model;
    model.kind = ElementKind::Solid;
    model.materials = {material};
    const Vec3 e1 = axis1.normalized(), e2 = axis2.normalized(), e3 = axis3.normalized();
    for (int k = 0; k <= n3; ++k)
        for (int j = 0; j <= n2; ++j)
            for (int i = 0; i <= n1; ++i)
                model.nodes.push_back(centre + size.x() * (static_cast<double>(i) / n1 - 0.5) * e1
                                      + size.y() * (static_cast<double>(j) / n2 - 0.5) * e2
                                      + size.z() * (static_cast<double>(k) / n3 - 0.5) * e3);
    const auto id = [&](int i, int j, int k) { return i + (n1 + 1) * (j + (n2 + 1) * k); };
    for (int k = 0; k < n3; ++k)
        for (int j = 0; j < n2; ++j)
            for (int i = 0; i < n1; ++i) {
                Element e;
                e.nodes = {id(i, j, k), id(i + 1, j, k), id(i + 1, j + 1, k), id(i, j + 1, k),
                           id(i, j, k + 1), id(i + 1, j, k + 1), id(i + 1, j + 1, k + 1), id(i, j + 1, k + 1)};
                model.elements.push_back(e);
            }
    model.fixed.assign(model.nodes.size(), 0);
    return model;
}

Section rectangularSection(double width, double height)
{
    Section s;
    const double b = std::max(width, height), t = std::min(width, height);
    s.area = width * height;
    s.iz = height * width * width * width / 12.0;  // bending in the x-y plane: the width is along y
    s.iy = width * height * height * height / 12.0;
    // Saint-Venant torsion constant of a rectangle (series approximation,
    // within 0.5 % of the exact series for every aspect ratio).
    const double r = t / b;
    s.torsion = b * t * t * t * (1.0 / 3.0 - 0.21 * r * (1.0 - r * r * r * r / 12.0));
    s.polar = s.iy + s.iz;
    s.cy = width / 2.0;
    s.cz = height / 2.0;
    char text[120];
    std::snprintf(text, sizeof text, "rectangle %.4g x %.4g mm", width * 1e3, height * 1e3);
    s.description = text;
    return s;
}

Section circularSection(double radius)
{
    Section s;
    s.area = pi * radius * radius;
    s.iy = s.iz = pi * std::pow(radius, 4) / 4.0;
    s.torsion = s.polar = pi * std::pow(radius, 4) / 2.0;
    s.cy = s.cz = radius;
    s.circular = true;
    char text[80];
    std::snprintf(text, sizeof text, "circle, diameter %.4g mm", 2e3 * radius);
    s.description = text;
    return s;
}

Section tubeSection(double outerRadius, double innerRadius)
{
    Section s;
    s.area = pi * (outerRadius * outerRadius - innerRadius * innerRadius);
    s.iy = s.iz = pi * (std::pow(outerRadius, 4) - std::pow(innerRadius, 4)) / 4.0;
    s.torsion = s.polar = 2.0 * s.iy;
    s.cy = s.cz = outerRadius;
    s.circular = true;
    char text[100];
    std::snprintf(text, sizeof text, "tube, diameter %.4g / %.4g mm", 2e3 * outerRadius, 2e3 * innerRadius);
    s.description = text;
    return s;
}

void fixDofs(StructuralModel& model, size_t node, std::initializer_list<int> dofs)
{
    for (const int d : dofs)
        model.fix(node, d);
}

void fixAll(StructuralModel& model, size_t node)
{
    for (int d = 0; d < model.dofsPerNode(); ++d)
        model.fix(node, d);
}

}  // namespace resonant::mechanics
