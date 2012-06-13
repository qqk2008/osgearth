/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
* Copyright 2008-2012 Pelican Mapping
* http://osgearth.org
*
* osgEarth is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>
*
* Original author: Thomas Lerman
*/
#include <osgEarthUtil/CMYKColorFilter>
#include <osgEarth/ShaderComposition>
#include <osgEarth/StringUtils>
#include <osgEarth/ThreadingUtils>
#include <osg/Program>
#include <OpenThreads/Atomic>

using namespace osgEarth;
using namespace osgEarth::Util;

namespace
{
    static OpenThreads::Atomic s_uniformNameGen;

    static const char* s_localShaderSource =
        "#version 110\n"
        "uniform vec4 __UNIFORM_NAME__;\n"

        "void __ENTRY_POINT__(in int slot, inout vec4 color)\n"
        "{\n"
        // apply cmy (negative of rgb):
        "   color.rgb -= __UNIFORM_NAME__.xyz; \n"
        // apply black (applies to all colors):
        "   color.rgb -= __UNIFORM_NAME__.w; \n"
        "   color.rgb = clamp(color.rgb, 0.0, 1.0); \n"
        "}\n";
}

//---------------------------------------------------------------------------

#define FUNCTION_PREFIX "osgearthutil_cmykColorFilter_"
#define UNIFORM_PREFIX  "osgearthutil_u_cmyk_"

//---------------------------------------------------------------------------

CMYKColorFilter::CMYKColorFilter(void)
{
    // Generate a unique name for this filter's uniform. This is necessary
    // so that each layer can have a unique uniform and entry point.
    m_instanceId = (++s_uniformNameGen) - 1;
    m_cmyk = new osg::Uniform(osg::Uniform::FLOAT_VEC4, (osgEarth::Stringify() << UNIFORM_PREFIX << m_instanceId));
    m_cmyk->set(osg::Vec4f(0.0f, 0.0f, 0.0f, 0.0f));
}

// CMY (without the K): http://forums.adobe.com/thread/428899
void CMYKColorFilter::setCMYOffset(const osg::Vec3f& value)
{
    osg::Vec4f cmyk;
    // find the minimum of all values
    cmyk[3] = 1.0;
    if (value[0] < cmyk[3])
    {
        cmyk[3] = value[0];
    }
    if (value[1] < cmyk[3])
    {
        cmyk[3] = value[1];
    }
    if (value[2] < cmyk[3])
    {
        cmyk[3] = value[2];
    }

    if (cmyk[3] == 1.0)
    {	// black
        cmyk[0] = cmyk[1] = cmyk[2] = 0.0;
    }
    else
    {
        cmyk[0] = (value[0] - cmyk[3]) / (1.0 - cmyk[3]);
        cmyk[1] = (value[1] - cmyk[3]) / (1.0 - cmyk[3]);
        cmyk[2] = (value[2] - cmyk[3]) / (1.0 - cmyk[3]);
    }

    setCMYKOffset(cmyk);
}

osg::Vec3f CMYKColorFilter::getCMYOffset(void) const
{
    osg::Vec4f cmyk = getCMYKOffset();
    osg::Vec3f cmy;

    if (cmyk[3] == 1.0)
    {
        cmy[0] = cmy[1] = cmy[2] = 1.0;
    }
    else
    {
        cmy[0] = (cmyk[0] * (1.0 - cmyk[3])) + cmyk[3];
        cmy[1] = (cmyk[1] * (1.0 - cmyk[3])) + cmyk[3];
        cmy[2] = (cmyk[2] * (1.0 - cmyk[3])) + cmyk[3];
    }
    return (cmy);
}

void CMYKColorFilter::setCMYKOffset(const osg::Vec4f& value)
{
    m_cmyk->set(value);
}

osg::Vec4f CMYKColorFilter::getCMYKOffset(void) const
{
    osg::Vec4f value;
    m_cmyk->get(value);
    return (value);
}

std::string CMYKColorFilter::getEntryPointFunctionName(void) const
{
    return (osgEarth::Stringify() << FUNCTION_PREFIX << m_instanceId);
}

void CMYKColorFilter::install(osg::StateSet* stateSet) const
{
    // safe: will not add twice.
    stateSet->addUniform(m_cmyk.get());

    osgEarth::VirtualProgram* vp = dynamic_cast<osgEarth::VirtualProgram*>(stateSet->getAttribute(osg::StateAttribute::PROGRAM));
    if (vp)
    {
        // build the local shader (unique per instance). We will
        // use a template with search and replace for this one.
        std::string entryPoint = osgEarth::Stringify() << FUNCTION_PREFIX << m_instanceId;
        std::string code = s_localShaderSource;
        osgEarth::replaceIn(code, "__UNIFORM_NAME__", m_cmyk->getName());
        osgEarth::replaceIn(code, "__ENTRY_POINT__", entryPoint);

        osg::Shader* main = new osg::Shader(osg::Shader::FRAGMENT, code);
        //main->setName(entryPoint);
        vp->setShader(entryPoint, main);
    }
}
