# IW4x Linking Exception

Version 1.1, 12 August 2026

## Preamble

This IW4x Linking Exception (the "Exception") is an additional permission under section 7 of the GNU General Public License, version 3, dated 29 June 2007 ("GPLv3").

The purpose of this Exception is to permit Covered IW4x Code to be combined with separately licensed IW4x Libraries, including private IW4x Libraries, without requiring Library Code to be licensed or disclosed under GPLv3 solely because of that permitted combination.

In particular, this Exception permits Covered IW4x Code to be statically linked with an IW4x Library and permits the resulting Combined Work to be conveyed while the IW4x Library remains governed by its separate license terms.

This Exception does not alter the GPLv3 status of Covered IW4x Code or Derived Covered Code. It grants no rights in an IW4x Library except the additional permission, granted by the applicable Licensor of Covered IW4x Code, to combine Covered IW4x Code with a qualifying IW4x Library as stated below.

## 1. Definitions

### 1.1. Covered IW4x Code

"Covered IW4x Code" means copyrightable material that is licensed under GPLv3 together with this Exception by a copyright holder having authority to grant this Exception with respect to that material.

Material is Covered IW4x Code only to the extent that this Exception has been validly applied to that material by a person or legal entity possessing the necessary copyright authority.

### 1.2. Licensor

"Licensor" means, with respect to particular Covered IW4x Code, a copyright holder that has validly applied this Exception to that Covered IW4x Code, or a person or legal entity expressly authorized by that copyright holder to grant the permissions stated in this Exception on the copyright holder's behalf.

A Licensor acts only within the copyright or licensing authority actually possessed by that Licensor. Nothing in this Exception grants authority over rights owned or controlled by another person or legal entity.

### 1.3. IW4x Project Authority

"IW4x Project Authority" means, with respect to particular Covered IW4x Code, the person, persons, or legal entity that is authorized by the applicable Licensor or Licensors to approve official IW4x releases containing that Covered IW4x Code and to designate software as an official IW4x component for purposes of this Exception.

Such authority must arise from an express delegation, an applicable assignment or license of rights, or project-governance authority accepted by the applicable Licensor or Licensors. Authority may be evidenced by licensing notices, project-governance materials, official release records, signed release metadata, or another written record that objectively identifies the authority.

A person or legal entity does not become IW4x Project Authority merely because that person or entity:

1. owns copyright in a candidate library or other candidate work;
2. contributes to, forks, hosts, mirrors, packages, or distributes IW4x software;
3. controls a repository, organization account, domain name, package namespace, or distribution channel;
4. uses an IW4x-related name, mark, identifier, or naming convention; or
5. can technically combine a work with Covered IW4x Code.

The identity of the IW4x Project Authority does not depend upon any particular source-control host, repository, organization account, domain name, package registry, or filesystem location. Receipt of Covered IW4x Code or of this Exception does not itself delegate IW4x Project Authority to a recipient.

### 1.4. Authorized IW4x Status

A work has "Authorized IW4x Status," with respect to particular Covered IW4x Code, only if the IW4x Project Authority authorized with respect to that Covered IW4x Code has affirmatively identified the work as an official IW4x component and the work has a substantive IW4x project relationship independent of that designation. Authorized IW4x Status is therefore evaluated with respect to the Covered IW4x Code for which this Exception is being relied upon.

A substantive IW4x project relationship exists only where the work was authored, commissioned, or materially developed under the direction of the IW4x Project Authority for use as an IW4x component, or where rights in the work were assigned or expressly licensed to the IW4x Project Authority specifically for use or maintenance as an IW4x component. A general-purpose third-party license made available to the public, standing alone, does not establish that relationship.

The affirmative identification must be objectively evidenced by at least one of the following acts performed by the IW4x Project Authority:

1. publishing or releasing the work as an IW4x-maintained component of an official IW4x release;
2. incorporating the work into an official IW4x distribution and expressly identifying it as an IW4x-maintained component;
3. expressly designating the work in writing as an official IW4x component; or
4. authoring or commissioning the work for IW4x and contemporaneously identifying it in release, licensing, build, or project-governance material as an official IW4x component.

A designation may identify a component by name, version, build identifier, source revision, cryptographic digest, or another identifier sufficient to distinguish the designated work from unrelated works.

A designation of a component family does not give Authorized IW4x Status to an independently modified version, fork, or successor merely because it uses the same name or descends from a designated version. A later version has Authorized IW4x Status only if it is itself released, adopted, or otherwise affirmatively approved by the IW4x Project Authority, unless the original designation expressly and unambiguously covers that later version.

Repository placement, naming, compatibility, technical integration, build dependency, distribution alongside IW4x, or ownership of copyright in the candidate work is insufficient by itself to establish Authorized IW4x Status.

### 1.5. IW4x Work

"IW4x Work" means a copyrightable software work, or an identifiable software component of a work, that has Authorized IW4x Status.

An IW4x Work may be maintained publicly or privately. It may be maintained in any repository or without a network-accessible repository. It may be distributed in source form, object-code form, executable form, or another form. It may be governed by GPLv3 or by different lawful license terms.

A work does not become an IW4x Work solely because it:

1. uses "IW4x" or an IW4x-related word, mark, name, or identifier;
2. uses an `iw4x`, `iw4x-*`, `libiw4x`, `libiw4x-*`, or similar naming convention;
3. is compatible or interoperable with IW4x;
4. implements an interface used or exposed by IW4x;
5. links with or can be linked with Covered IW4x Code;
6. is loaded, invoked, called, or otherwise used by Covered IW4x Code;
7. is distributed together with an IW4x Work;
8. appears in the same repository, source tree, package, archive, build, or distribution as an IW4x Work;
9. is a dependency of an IW4x Work;
10. is a fork, copy, modification, or derivative of software associated with IW4x; or
11. is represented as official by a person lacking IW4x Project Authority.

### 1.6. IW4x Library

"IW4x Library" means an IW4x Work that is a separately compiled, separately linkable, or separately loadable software component intended by the IW4x Project Authority to be incorporated into, linked with, or loaded by another IW4x Work as part of the same executable image or process.

An IW4x Library may be combined with another IW4x Work through static linking, dynamic linking, object-code incorporation, link-time code generation or optimization, runtime loading into the same process, or another technically equivalent in-process mechanism.

Communication between separate processes solely through files, pipes, sockets, remote procedure calls, network protocols, or other inter-process communication does not by itself make either process an IW4x Library under this definition.

An IW4x Library may be public or private and may be governed by license terms different from GPLv3.

For the avoidance of doubt, a private transitional library such as `libiw4x-unstable` qualifies as an IW4x Library when it has Authorized IW4x Status and otherwise satisfies this definition.

### 1.7. Library Code

"Library Code" means copyrightable material contained in an IW4x Library that forms part of the substantive IW4x implementation of that library and is neither Covered IW4x Code nor Derived Covered Code.

Material does not become Library Code merely because it is vendored, bundled, embedded, archived, statically linked, or otherwise packaged inside an IW4x Library. Third-party material that lacks the substantive IW4x project relationship required by section 1.4 is not Library Code for purposes of this Exception unless that material independently acquires Authorized IW4x Status.

Library Code remains governed by the license terms otherwise applicable to that material. This Exception does not supply, replace, broaden, or waive those separate license terms.

Third-party material contained in an IW4x Library remains subject to its own license terms and does not acquire any additional rights under this Exception.

### 1.8. Derived Covered Code

"Derived Covered Code" means copyrightable material that modifies, adapts, copies, or is based upon Covered IW4x Code to an extent that makes the material subject to GPLv3 under GPLv3 and applicable copyright law.

Derived Covered Code remains subject to GPLv3 to the extent required by GPLv3.

Derived Covered Code does not become Library Code merely because it is moved, renamed, separately compiled, incorporated into an IW4x Library, placed in a private repository, or combined through an IW4x Library.

This Exception applies to Derived Covered Code only to the extent that the applicable copyright holder has validly made this Exception applicable to that material and the Exception has not been removed from that material in accordance with GPLv3 section 7.

### 1.9. Combined Work

"Combined Work" means an executable program, library, or other software work produced by combining Covered IW4x Code with one or more IW4x Libraries so that Covered IW4x Code and Library Code form or participate in the same executable image or process.

A Combined Work includes a work produced through static linking, dynamic linking, object-code incorporation, link-time code generation or optimization, runtime loading into the same process, or another technically equivalent in-process mechanism.

An executable containing Covered IW4x Code and Library Code as a consequence of static linking is a Combined Work.

Mere aggregation on the same storage or distribution medium, and communication between separate processes solely through inter-process or network interfaces, do not by themselves create a Combined Work under this Exception.

## 2. Grant of Additional Permission

Subject to the conditions of this Exception, each applicable Licensor grants additional permission to combine that Licensor's Covered IW4x Code with one or more IW4x Libraries to produce a Combined Work and to convey the resulting Combined Work.

For a Combined Work made and conveyed in accordance with this Exception, you may satisfy GPLv3 section 5 without licensing Library Code under GPLv3 solely because Library Code forms part of the Combined Work. In particular, the requirement in GPLv3 section 5(c) to license the work as a whole under GPLv3 is relaxed, solely with respect to Library Code, to the extent necessary to permit Library Code to remain governed by its separate license terms.

You may statically link Covered IW4x Code with an IW4x Library and convey the resulting Combined Work under those separate licensing arrangements.

The act of combining Covered IW4x Code with an IW4x Library under this Exception does not, by itself, relicense Library Code under GPLv3.

No permission is granted to remove GPLv3 from Covered IW4x Code or Derived Covered Code. Except for the combinations with qualifying IW4x Libraries expressly authorized by this Exception, this Exception grants no general permission to combine Covered IW4x Code with non-GPLv3 software.

## 3. Conditions of the Additional Permission

You may rely upon this Exception only if all of the following conditions are satisfied:

1. you comply with GPLv3 with respect to Covered IW4x Code and Derived Covered Code except only for the obligations expressly relaxed by this Exception;
2. each library relied upon as an IW4x Library has Authorized IW4x Status, with respect to every item of Covered IW4x Code for which this Exception is necessary, for the version or build actually combined;
3. you comply with the separate license terms applicable to each IW4x Library and to all Library Code contained in it;
4. you possess all permissions independently required to reproduce, modify, link, load, combine, use, and convey each IW4x Library in the manner undertaken;
5. no term applicable to an IW4x Library causes you to violate GPLv3 with respect to Covered IW4x Code or Derived Covered Code; and
6. you do not classify, relabel, move, copy, refactor, or package Covered IW4x Code or Derived Covered Code as Library Code for the purpose of avoiding GPLv3 obligations.

A distributor relying upon this Exception must be able to identify the IW4x Library version or build relied upon with sufficient specificity to establish that the combined library possessed Authorized IW4x Status. This requirement may be satisfied through release metadata, build metadata, a source revision, a version or build identifier, a cryptographic digest, or another durable record. It does not require publication of private Library Code.

This Exception grants additional permission under GPLv3. It grants no permission under the separate license applicable to an IW4x Library.

## 4. Effect on IW4x Libraries

Library Code does not become subject to GPLv3 solely because it is combined with Covered IW4x Code in a Combined Work authorized by this Exception.

An IW4x Library may remain governed by its separate license terms when incorporated into a Combined Work. Those terms may govern access to the source code of the IW4x Library and may lawfully keep that source code private.

This Exception does not alter any obligation independently imposed upon Library Code by its own license, by contract, or by applicable law.

## 5. Static Linking

The additional permission granted by this Exception expressly applies to static linking.

Covered IW4x Code may be statically linked with one or more IW4x Libraries to produce a single executable or other binary work, and that Combined Work may be conveyed without Library Code becoming subject to GPLv3 solely because of the static linkage.

A distributor is not required under GPLv3, solely because of static linkage permitted by this Exception, to license, disclose, or convey the source code of Library Code.

A distributor is not required under GPLv3, solely because of that static linkage, to provide object files, relinking material, intermediate representations, or build material belonging exclusively to Library Code.

The GPLv3 obligations applicable to Covered IW4x Code and Derived Covered Code continue to apply except to the extent expressly relaxed by this Exception.

## 6. Corresponding Source

When a Combined Work is conveyed in reliance upon this Exception, the distributor must provide all Corresponding Source required by GPLv3 for Covered IW4x Code and Derived Covered Code, subject only to the exclusions expressly stated in this section.

For purposes of conveying a Combined Work under GPLv3 section 6, the distributor may omit source code and other material belonging exclusively to Library Code when the obligation to provide that material would arise solely from the permitted combination of Library Code with Covered IW4x Code.

The permitted omission includes, to that extent:

1. source code belonging exclusively to Library Code;
2. object files belonging exclusively to Library Code;
3. intermediate representations belonging exclusively to Library Code;
4. private interface material belonging exclusively to Library Code;
5. scripts, configuration, or build material used exclusively to produce Library Code; and
6. other non-public implementation material belonging exclusively to Library Code.

Material is not "exclusively" Library Code merely because it is stored in an IW4x Library. Covered IW4x Code and Derived Covered Code remain subject to the GPLv3 Corresponding Source obligation even when compiled into, linked through, or distributed as part of an IW4x Library.

Nothing in this section excludes source code, interface definitions, scripts, or other material to the extent that the material is itself Covered IW4x Code or Derived Covered Code and GPLv3 requires it as Corresponding Source for that covered material.

## 7. Installation Information and Relinking

To the extent GPLv3 section 6 would otherwise require Installation Information, object files, relinking material, authorization information, or build information belonging exclusively to Library Code solely because an IW4x Library forms part of a Combined Work, this Exception grants additional permission to convey the Combined Work without providing that Library Code material.

This permission does not waive GPLv3 obligations concerning Covered IW4x Code or Derived Covered Code when the required information can be provided without disclosing, reconstructing, or supplying material belonging exclusively to Library Code.

Nothing in this section requires disclosure of a private signing key, private encryption key, credential, or other secret merely because that secret is used exclusively to protect or produce Library Code. This sentence does not waive an Installation Information obligation that GPLv3 independently imposes with respect to Covered IW4x Code and that can be satisfied without disclosure of Library Code.

## 8. Separation of Licensing Status

The classification of software as an IW4x Work or IW4x Library does not alter the copyright or licensing status that the software otherwise possesses.

Covered IW4x Code remains Covered IW4x Code. Derived Covered Code remains subject to GPLv3 to the extent required by GPLv3. Library Code remains subject to its separate license terms.

Physical location, repository placement, packaging, compilation boundaries, linkage, naming, or distribution format does not reclassify Covered IW4x Code or Derived Covered Code as Library Code.

## 9. No Conversion or Laundering of Covered Code

Nothing in this Exception permits Covered IW4x Code or Derived Covered Code to be converted into privately licensed Library Code through a technical, organizational, repository, packaging, or licensing-label change.

Covered IW4x Code or Derived Covered Code does not become Library Code merely because it is moved into a private repository, moved into an IW4x Library, incorporated into a new IW4x Work, renamed, separately compiled, statically linked, dynamically linked, reorganized, or made unavailable to the public.

A substantial rewrite, refactor, translation, generated form, or reimplementation does not become Library Code to the extent that it remains a modification of or work based on Covered IW4x Code under GPLv3 and applicable copyright law.

## 10. Third-Party Software

Third-party software does not acquire Authorized IW4x Status merely because the IW4x Project Authority possesses, hosts, mirrors, vendors, bundles, distributes, depends upon, patches, or interoperates with that software.

A third-party dependency, fork, mirror, vendor copy, bundled library, compatibility layer, SDK, runtime, or other third-party component remains governed by its applicable license terms. It does not become Library Code merely because an IW4x Library contains or depends upon it.

Third-party material may qualify for this Exception only if it independently satisfies every requirement for Authorized IW4x Status, including the substantive IW4x project relationship required by section 1.4. Mere adoption, bundling, or designation of an otherwise unrelated third-party component is insufficient.

This Exception grants no copyright, patent, trademark, contractual, or other permission in third-party software. The distributor must independently possess all rights necessary to combine and convey third-party material contained in or used by an IW4x Library.

## 11. Forks and Unofficial Works

A fork, copy, modification, derivative, or successor of an IW4x Work does not automatically possess Authorized IW4x Status.

A third party cannot create Authorized IW4x Status by copying a repository, preserving an IW4x-related name, maintaining compatibility, reproducing release metadata, controlling a mirror, publishing a build, or representing a work as official.

A forked or modified version acquires Authorized IW4x Status only through an affirmative act of the IW4x Project Authority satisfying section 1.4.

## 12. Repository, Organization, and Hosting Independence

No permission granted by this Exception depends upon a work being hosted in a particular repository, organization account, namespace, domain, package registry, source-control provider, distribution service, or legal entity.

Transfer of an otherwise qualifying IW4x Work between repositories or hosting services does not by itself terminate its Authorized IW4x Status.

Transfer of a non-qualifying work into a repository, organization, namespace, server, account, domain, package, or distribution used by IW4x does not by itself create Authorized IW4x Status.

A repository or distribution may contain both IW4x Works and material that is not an IW4x Work.

## 13. Continuity and Version Scope of Authorized IW4x Status

Once a particular version or build of a work has validly acquired Authorized IW4x Status, later discontinuation, archival, relocation, renaming, supersession, or cessation of maintenance does not retroactively extinguish that status for that version or build.

A transfer of ownership or control does not retroactively invalidate an exercise of this Exception that was valid when undertaken.

Authorized IW4x Status does not automatically pass to a later modification, fork, successor, derivative, or independently produced build. Each such version must independently satisfy section 1.4 unless an existing designation expressly and unambiguously covers it.

The separate license governing an IW4x Library may independently limit whether a recipient may continue to possess, use, reproduce, modify, or convey that library. This Exception does not override such limits.

## 14. Preservation of GPLv3

Except for the additional permissions expressly granted by this Exception, GPLv3 applies without modification to Covered IW4x Code and Derived Covered Code.

This Exception does not authorize the imposition of an additional restriction upon rights granted by GPLv3 with respect to Covered IW4x Code or Derived Covered Code.

A recipient of Covered IW4x Code retains the rights granted by GPLv3 as supplemented by this Exception to the extent the Exception remains applicable to the material received.

A license governing an IW4x Library does not become a term governing Covered IW4x Code merely because the library participates in a Combined Work.

If a Combined Work cannot be conveyed while complying simultaneously with GPLv3 as supplemented by this Exception and with all terms applicable to the IW4x Libraries and other material contained in the Combined Work, this Exception grants no permission to convey that Combined Work.

## 15. Scope of Authority

This Exception applies to Covered IW4x Code only to the extent that the applicable Licensor possesses authority to grant the permissions stated here.

The presence of this Exception in a repository, source distribution, binary distribution, package, archive, build, or other collection does not grant permission over material whose copyright holder has not validly made this Exception applicable.

Where multiple copyright holders possess rights in Covered IW4x Code, each grant operates only within the authority of the copyright holder making or authorizing that grant.

No provision of this Exception purports to relicense copyright owned by a person who has not granted or authorized the relevant permission.

## 16. Removal of the Additional Permission

GPLv3 section 7 permits a recipient conveying a covered work to remove additional permissions from that copy or from part of it.

To the extent a recipient validly removes this Exception from particular material, that recipient may not rely upon this Exception as authority to combine that material with an IW4x Library when such permission would otherwise be required.

Removal from particular material does not remove this Exception from other material to which it continues validly to apply.

Removal from a later copy or modification does not retroactively invalidate an act that was authorized when performed under a copy to which this Exception applied.

## 17. No Trademark License

Nothing in GPLv3 or this Exception grants permission to use any trademark, service mark, trade name, logo, product identity, or other designation of origin associated with IW4x except to the extent such use is independently permitted by applicable law or by the holder of the relevant rights.

The terms "IW4x," "IW4x Project Authority," "IW4x Work," and "IW4x Library" are used in this Exception solely to define the software relationships governed by the additional permission.

## 18. Patents and No Implied Rights

This Exception does not enlarge any patent license granted by GPLv3 with respect to Covered IW4x Code or Derived Covered Code.

No patent license in Library Code, in an IW4x Library, or in a combination with an IW4x Library is granted by this Exception except to the extent a patent right is independently granted under GPLv3, the separate license governing the relevant material, or another express authorization from the patent holder.

No copyright license, patent license, trademark license, waiver, sublicense, covenant, authorization, or other right in an IW4x Library is granted by implication from this Exception.

Nothing in this section limits rights that applicable law grants independently of copyright or license terms.

## 19. Construction and Version

Defined terms used in this Exception have the meanings assigned to them by this Exception. Terms defined by GPLv3 and used here without a separate definition retain their GPLv3 meanings.

This Exception supplements GPLv3 and is intended to operate solely as an additional permission under GPLv3 section 7.

References to GPLv3 sections refer to GNU General Public License version 3 dated 29 June 2007.

This Exception is version 1.1. A notice applying this Exception applies version 1.1 only unless that notice expressly authorizes use of a later version of the IW4x Linking Exception.

If Covered IW4x Code is licensed under "GPLv3 or any later version," this Exception does not automatically apply when a recipient elects to use a later version of the GNU General Public License unless the applicable licensing notice expressly states that the Exception applies to that later GPL version.

No informal description, repository layout, source-tree organization, build configuration, naming convention, development practice, or technical implementation enlarges the permissions granted by this Exception.

If an informal description differs from the operative text of this Exception, the operative text controls.

## 20. Severability

If a provision of this Exception is determined to be invalid or unenforceable under applicable law, that provision is ineffective only to the extent of that invalidity or unenforceability.

The remaining provisions continue to describe the additional permissions granted by each applicable Licensor to the maximum extent permitted by applicable law.

Nothing in this section expands the authority of a Licensor or creates a permission that the Licensor lacks authority to grant.

## 21. Application of this Exception

This Exception may be applied to Covered IW4x Code only by a copyright holder possessing the necessary authority, or by a person or legal entity expressly authorized by that copyright holder.

The application notice must identify the material to which the Exception applies with reasonable clarity. It may do so through a source-file notice, a repository-wide licensing notice, or another written notice distributed with the material.

A repository-wide notice applies only to material whose copyright holders have authorized that repository-wide licensing treatment. It does not override a file-specific license, a third-party notice, or another indication that particular material is governed by different terms.

A recommended repository-wide notice is:

> IW4x is licensed under the GNU General Public License, version 3, subject to the additional permissions described in version 1.1 of the IW4x Linking Exception.
>
> See `LICENSE.md` and `LICENSE-EXCEPTION.md`.

A recommended source-file notice is:

> This file is part of IW4x.
>
> This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License, version 3, as published by the Free Software Foundation, subject to the additional permissions described in version 1.1 of the IW4x Linking Exception.
>
> You should have received a copy of the GNU General Public License and the IW4x Linking Exception along with this program.

The absence of a per-file notice does not prevent repository-wide application when the applicable copyright holder has clearly and validly made the repository-wide notice applicable to that material.

## 22. Entire Additional Permission and Independent Grants

This Exception constitutes the complete statement of the additional permission granted by its terms concerning the combination of Covered IW4x Code with IW4x Libraries.

No repository location, organization membership, source-control configuration, library name, private or public development status, technical dependency, release practice, or other circumstance creates permission under this Exception independently of its operative terms.

Nothing in this Exception limits a copyright holder's ability to grant a separate license or separate permission outside this Exception with respect to rights that copyright holder controls. Any such separate grant stands on its own terms and is not enlarged by this Exception.

Except for this Exception and any independently valid separate grant, the rights and obligations concerning Covered IW4x Code are determined by GPLv3.
