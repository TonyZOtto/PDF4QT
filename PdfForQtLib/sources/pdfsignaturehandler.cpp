//    Copyright (C) 2020 Jakub Melka
//
//    This file is part of PdfForQt.
//
//    PdfForQt is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Lesser General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    PdfForQt is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public License
//    along with PDFForQt.  If not, see <https://www.gnu.org/licenses/>.

#include "pdfsignaturehandler.h"
#include "pdfdocument.h"
#include "pdfencoding.h"
#include "pdfform.h"
#include "pdfutils.h"
#include "pdfsignaturehandler_impl.h"

#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/rsaerr.h>

#include <QMutex>
#include <QMutexLocker>
#include <QDataStream>

#include <array>

namespace pdf
{

static QMutex s_globalOpenSSLMutex(QMutex::Recursive);

/// OpenSSL is not thread safe.
class PDFOpenSSLGlobalLock
{
public:
    explicit inline PDFOpenSSLGlobalLock() : m_mutexLocker(&s_globalOpenSSLMutex) { }
    inline ~PDFOpenSSLGlobalLock() = default;

private:
    QMutexLocker m_mutexLocker;
};

PDFSignatureReference PDFSignatureReference::parse(const PDFObjectStorage* storage, PDFObject object)
{
    PDFSignatureReference result;

    if (const PDFDictionary* dictionary = storage->getDictionaryFromObject(object))
    {
        PDFDocumentDataLoaderDecorator loader(storage);

        constexpr const std::array<std::pair<const char*, PDFSignatureReference::TransformMethod>, 3> types = {
            std::pair<const char*, PDFSignatureReference::TransformMethod>{ "DocMDP", PDFSignatureReference::TransformMethod::DocMDP },
            std::pair<const char*, PDFSignatureReference::TransformMethod>{ "UR", PDFSignatureReference::TransformMethod::UR },
            std::pair<const char*, PDFSignatureReference::TransformMethod>{ "FieldMDP", PDFSignatureReference::TransformMethod::FieldMDP }
        };

        // Jakub Melka: parse the signature reference dictionary
        result.m_transformMethod = loader.readEnumByName(dictionary->get("TransformMethod"), types.cbegin(), types.cend(), PDFSignatureReference::TransformMethod::Invalid);
        result.m_transformParams = dictionary->get("TransformParams");
        result.m_data = dictionary->get("Data");
        result.m_digestMethod = loader.readNameFromDictionary(dictionary, "DigestMethod");
    }

    return result;
}

PDFSignature PDFSignature::parse(const PDFObjectStorage* storage, PDFObject object)
{
    PDFSignature result;

    if (const PDFDictionary* dictionary = storage->getDictionaryFromObject(object))
    {
        PDFDocumentDataLoaderDecorator loader(storage);

        constexpr const std::array<std::pair<const char*, Type>, 2> types = {
            std::pair<const char*, Type>{ "Sig", Type::Sig },
            std::pair<const char*, Type>{ "DocTimeStamp", Type::DocTimeStamp }
        };

        // Jakub Melka: parse the signature dictionary
        result.m_type = loader.readEnumByName(dictionary->get("Type"), types.cbegin(), types.cend(), Type::Sig);
        result.m_filter = loader.readNameFromDictionary(dictionary, "Filter");
        result.m_subfilter = loader.readNameFromDictionary(dictionary, "SubFilter");
        result.m_contents = loader.readStringFromDictionary(dictionary, "Contents");

        if (dictionary->hasKey("Cert"))
        {
            PDFObject certificates = storage->getObject(dictionary->get("Cert"));
            if (certificates.isString())
            {
                result.m_certificates = { loader.readString(certificates) };
            }
            else if (certificates.isArray())
            {
                result.m_certificates = loader.readStringArray(certificates);
            }
        }

        std::vector<PDFInteger> byteRangesArray = loader.readIntegerArrayFromDictionary(dictionary, "ByteRange");
        const size_t byteRangeCount = byteRangesArray.size() / 2;
        result.m_byteRanges.reserve(byteRangeCount);
        for (size_t i = 0; i < byteRangeCount; ++i)
        {
            ByteRange byteRange = { byteRangesArray[2 * i], byteRangesArray[2 * i + 1] };
            result.m_byteRanges.push_back(byteRange);
        }

        result.m_references = loader.readObjectList<PDFSignatureReference>(dictionary->get("References"));
        std::vector<PDFInteger> changes = loader.readIntegerArrayFromDictionary(dictionary, "Changes");

        if (changes.size() == 3)
        {
            result.m_changes = { changes[0], changes[1], changes[2] };
        }

        result.m_name = loader.readTextStringFromDictionary(dictionary, "Name", QString());
        result.m_signingDateTime = PDFEncoding::convertToDateTime(loader.readStringFromDictionary(dictionary, "M"));
        result.m_location = loader.readTextStringFromDictionary(dictionary, "Location", QString());
        result.m_reason = loader.readTextStringFromDictionary(dictionary, "Reason", QString());
        result.m_contactInfo = loader.readTextStringFromDictionary(dictionary, "ContactInfo", QString());
        result.m_R = loader.readIntegerFromDictionary(dictionary, "R", 0);
        result.m_V = loader.readIntegerFromDictionary(dictionary, "V", 0);
        result.m_propBuild = dictionary->get("Prop_Build");
        result.m_propTime = loader.readIntegerFromDictionary(dictionary, "Prop_AuthTime", 0);

        constexpr const std::array<std::pair<const char*, AuthentificationType>, 3> authentificationTypes = {
            std::pair<const char*, AuthentificationType>{ "PIN", AuthentificationType::PIN },
            std::pair<const char*, AuthentificationType>{ "Password", AuthentificationType::Password },
            std::pair<const char*, AuthentificationType>{ "Fingerprint", AuthentificationType::Fingerprint }
        };
        result.m_propType = loader.readEnumByName(dictionary->get("Prop_AuthType"), authentificationTypes.cbegin(), authentificationTypes.cend(), AuthentificationType::Invalid);
    }

    return result;
}

PDFSignatureHandler* PDFSignatureHandler::createHandler(const PDFFormFieldSignature* signatureField, const QByteArray& sourceData, const Parameters& parameters)
{
    Q_ASSERT(signatureField);

    const QByteArray& subfilter = signatureField->getSignature().getSubfilter();
    if (subfilter == "adbe.pkcs7.detached")
    {
        return new PDFSignatureHandler_adbe_pkcs7_detached(signatureField, sourceData, parameters);
    }
    else if (subfilter == "adbe.pkcs7.sha1")
    {
        return new PDFSignatureHandler_adbe_pkcs7_sha1(signatureField, sourceData, parameters);
    }
    else if (subfilter == "adbe.x509.rsa_sha1")
    {
        return new PDFSignatureHandler_adbe_pkcs7_rsa_sha1(signatureField, sourceData, parameters);
    }

    return nullptr;
}

std::vector<PDFSignatureVerificationResult> PDFSignatureHandler::verifySignatures(const PDFForm& form, const QByteArray& sourceData, const Parameters& parameters)
{
    std::vector<PDFSignatureVerificationResult> result;

    if (parameters.enableVerification && (form.isAcroForm() || form.isXFAForm()))
    {
        std::vector<const PDFFormFieldSignature*> signatureFields;
        auto getSignatureFields = [&signatureFields](const PDFFormField* field)
        {
            if (field->getFieldType() == PDFFormField::FieldType::Signature)
            {
                const PDFFormFieldSignature* signatureField = dynamic_cast<const PDFFormFieldSignature*>(field);
                Q_ASSERT(signatureField);
                signatureFields.push_back(signatureField);
            }
        };
        form.apply(getSignatureFields);
        result.reserve(signatureFields.size());

        for (const PDFFormFieldSignature* signatureField : signatureFields)
        {
            if (const PDFSignatureHandler* signatureHandler = createHandler(signatureField, sourceData, parameters))
            {
                result.emplace_back(signatureHandler->verify());
                delete signatureHandler;
            }
            else
            {
                PDFObjectReference signatureFieldReference = signatureField->getSelfReference();
                QString qualifiedName = signatureField->getName(PDFFormField::NameType::FullyQualified);
                PDFSignatureVerificationResult verificationResult(signatureFieldReference, qMove(qualifiedName));
                verificationResult.addNoHandlerError(signatureField->getSignature().getSubfilter());
                result.emplace_back(qMove(verificationResult));
            }
        }
    }

    return result;
}

void PDFSignatureVerificationResult::addNoHandlerError(const QByteArray& format)
{
    m_flags.setFlag(Error_NoHandler);
    m_errors << PDFTranslationContext::tr("No signature handler for signature format '%1'.").arg(QString::fromLatin1(format));
}

void PDFSignatureVerificationResult::addInvalidCertificateError()
{
    m_flags.setFlag(Error_Certificate_Invalid);
    m_errors << PDFTranslationContext::tr("Certificate format is invalid.");
}

void PDFSignatureVerificationResult::addNoSignaturesError()
{
    m_flags.setFlag(Error_Certificate_NoSignatures);
    m_errors << PDFTranslationContext::tr("No signatures in certificate data.");
}

void PDFSignatureVerificationResult::addCertificateMissingError()
{
    m_flags.setFlag(Error_Certificate_Missing);
    m_errors << PDFTranslationContext::tr("Certificate is missing.");
}

void PDFSignatureVerificationResult::addCertificateGenericError()
{
    m_flags.setFlag(Error_Certificate_Generic);
    m_errors << PDFTranslationContext::tr("Generic error occured during certificate validation.");
}

void PDFSignatureVerificationResult::addCertificateExpiredError()
{
    m_flags.setFlag(Error_Certificate_Expired);
    m_errors << PDFTranslationContext::tr("Certificate has expired.");
}

void PDFSignatureVerificationResult::addCertificateSelfSignedError()
{
    m_flags.setFlag(Error_Certificate_SelfSigned);
    m_errors << PDFTranslationContext::tr("Certificate is self-signed.");
}

void PDFSignatureVerificationResult::addCertificateSelfSignedInChainError()
{
    m_flags.setFlag(Error_Certificate_SelfSignedChain);
    m_errors << PDFTranslationContext::tr("Self-signed certificate in chain.");
}

void PDFSignatureVerificationResult::addCertificateTrustedNotFoundError()
{
    m_flags.setFlag(Error_Certificate_TrustedNotFound);
    m_errors << PDFTranslationContext::tr("Trusted certificate not found.");
}

void PDFSignatureVerificationResult::addCertificateRevokedError()
{
    m_flags.setFlag(Error_Certificate_Revoked);
    m_errors << PDFTranslationContext::tr("Certificate has been revoked.");
}

void PDFSignatureVerificationResult::addCertificateOtherError(int error)
{
    m_flags.setFlag(Error_Certificate_Other);
    m_errors << PDFTranslationContext::tr("Certificate validation failed with code %1.").arg(error);
}

void PDFSignatureVerificationResult::addInvalidSignatureError()
{
    m_flags.setFlag(Error_Signature_Invalid);
    m_errors << PDFTranslationContext::tr("Signature is invalid.");
}

void PDFSignatureVerificationResult::addSignatureNoSignaturesFoundError()
{
    m_flags.setFlag(Error_Signature_NoSignaturesFound);
    m_errors << PDFTranslationContext::tr("No signatures found in certificate.");
}

void PDFSignatureVerificationResult::addSignatureCertificateMissingError()
{
    m_flags.setFlag(Error_Signature_SourceCertificateMissing);
    m_errors << PDFTranslationContext::tr("Signature certificate is missing.");
}

void PDFSignatureVerificationResult::addSignatureDigestFailureError()
{
    m_flags.setFlag(Error_Signature_DigestFailure);
    m_errors << PDFTranslationContext::tr("Signed data has different hash function digest.");
}

void PDFSignatureVerificationResult::addSignatureDataOtherError()
{
    m_flags.setFlag(Error_Signature_DataOther);
    m_errors << PDFTranslationContext::tr("Signed data are invalid.");
}

void PDFSignatureVerificationResult::addSignatureDataCoveredBySignatureMissingError()
{
    m_flags.setFlag(Error_Signature_DataCoveredBySignatureMissing);
    m_errors << PDFTranslationContext::tr("Data covered by signature are not present.");
}

void PDFSignatureVerificationResult::addSignatureNotCoveredBytesWarning(PDFInteger count)
{
    m_flags.setFlag(Warning_Signature_NotCoveredBytes);
    m_warnings << PDFTranslationContext::tr("%1 bytes are not covered by signature.").arg(count);
}

void PDFSignatureVerificationResult::setSignatureFieldQualifiedName(const QString& signatureFieldQualifiedName)
{
    m_signatureFieldQualifiedName = signatureFieldQualifiedName;
}

void PDFSignatureVerificationResult::setSignatureFieldReference(PDFObjectReference signatureFieldReference)
{
    m_signatureFieldReference = signatureFieldReference;
}

void PDFSignatureVerificationResult::validate()
{
    if (isCertificateValid() && isSignatureValid())
    {
        m_flags.setFlag(OK);
    }
}

void PDFPublicKeySignatureHandler::initializeResult(PDFSignatureVerificationResult& result) const
{
    PDFObjectReference signatureFieldReference = m_signatureField->getSelfReference();
    QString signatureFieldQualifiedName = m_signatureField->getName(PDFFormField::NameType::FullyQualified);
    result.setSignatureFieldReference(signatureFieldReference);
    result.setSignatureFieldQualifiedName(signatureFieldQualifiedName);
}

STACK_OF(X509)* PDFPublicKeySignatureHandler::getCertificates(PKCS7* pkcs7)
{
    if (!pkcs7)
    {
        return nullptr;
    }

    if (PKCS7_type_is_signed(pkcs7))
    {
        return pkcs7->d.sign->cert;
    }

    if (PKCS7_type_is_signedAndEnveloped(pkcs7))
    {
        return pkcs7->d.signed_and_enveloped->cert;
    }

    return nullptr;
}

void PDFPublicKeySignatureHandler::verifyCertificate(PDFSignatureVerificationResult& result) const
{
    PDFOpenSSLGlobalLock lock;

    OpenSSL_add_all_algorithms();

    const PDFSignature& signature = m_signatureField->getSignature();
    const QByteArray& content = signature.getContents();

    // Jakub Melka: we will try to get pkcs7 from signature, then
    // verify signer certificates.
    const unsigned char* data = reinterpret_cast<const unsigned char*>(content.data());
    if (PKCS7* pkcs7 = d2i_PKCS7(nullptr, &data, content.size()))
    {
        X509_STORE* store = X509_STORE_new();
        X509_STORE_CTX* context = X509_STORE_CTX_new();

        // Above functions can fail only if not enough memory. But in this
        // case, this library will crash anyway.
        Q_ASSERT(store);
        Q_ASSERT(context);

        addTrustedCertificates(store);

        STACK_OF(PKCS7_SIGNER_INFO)* signerInfo = PKCS7_get_signer_info(pkcs7);
        const int signerInfoCount = sk_PKCS7_SIGNER_INFO_num(signerInfo);
        STACK_OF(X509)* certificates = getCertificates(pkcs7);
        if (signerInfo && signerInfoCount > 0 && certificates)
        {
            for (int i = 0; i < signerInfoCount; ++i)
            {
                PKCS7_SIGNER_INFO* signerInfoValue = sk_PKCS7_SIGNER_INFO_value(signerInfo, i);
                PKCS7_ISSUER_AND_SERIAL* issuerAndSerial = signerInfoValue->issuer_and_serial;
                X509* signer = X509_find_by_issuer_and_serial(certificates, issuerAndSerial->issuer, issuerAndSerial->serial);

                if (!signer)
                {
                    result.addCertificateMissingError();
                    break;
                }

                if (!X509_STORE_CTX_init(context, store, signer, certificates))
                {
                    result.addCertificateGenericError();
                    break;
                }

                if (!X509_STORE_CTX_set_purpose(context, X509_PURPOSE_SMIME_SIGN))
                {
                    result.addCertificateGenericError();
                    break;
                }

                unsigned long flags = X509_V_FLAG_TRUSTED_FIRST;
                if (m_parameters.ignoreExpirationDate)
                {
                    flags |= X509_V_FLAG_NO_CHECK_TIME;
                }
                X509_STORE_CTX_set_flags(context, flags);

                int verificationResult = X509_verify_cert(context);
                if (verificationResult <= 0)
                {
                    int error = X509_STORE_CTX_get_error(context);
                    switch (error)
                    {
                        case X509_V_OK:
                            // Strange, this should not occur... when X509_verify_cert fails
                            break;

                        case X509_V_ERR_CERT_HAS_EXPIRED:
                            result.addCertificateExpiredError();
                            break;

                        case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
                            result.addCertificateSelfSignedError();
                            break;

                        case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
                            result.addCertificateSelfSignedInChainError();
                            break;

                        case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
                        case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
                            result.addCertificateTrustedNotFoundError();
                            break;

                        case X509_V_ERR_CERT_REVOKED:
                            result.addCertificateRevokedError();
                            break;

                        default:
                            result.addCertificateOtherError(error);
                            break;
                    }

                    // We will add certificate info for all certificates
                    const int count = sk_X509_num(certificates);
                    for (int i = 0; i < count; ++i)
                    {
                        result.addCertificateInfo(getCertificateInfo(sk_X509_value(certificates, i)));
                    }
                }
                else
                {
                    STACK_OF(X509)* validChain = X509_STORE_CTX_get0_chain(context);
                    const int count = sk_X509_num(validChain);
                    for (int i = 0; i < count; ++i)
                    {
                        result.addCertificateInfo(getCertificateInfo(sk_X509_value(validChain, i)));
                    }
                }
                X509_STORE_CTX_cleanup(context);
            }
        }
        else
        {
            result.addNoSignaturesError();
        }

        X509_STORE_CTX_free(context);
        X509_STORE_free(store);

        PKCS7_free(pkcs7);
    }
    else
    {
        result.addInvalidCertificateError();
    }

    if (!result.hasCertificateError())
    {
        result.setFlag(PDFSignatureVerificationResult::Certificate_OK, true);
    }
}

BIO* PDFPublicKeySignatureHandler::getSignedDataBuffer(pdf::PDFSignatureVerificationResult& result, QByteArray& outputBuffer) const
{
    const PDFSignature& signature = m_signatureField->getSignature();
    const QByteArray& contents = signature.getContents();
    const QByteArray& sourceData = m_sourceData;

    PDFInteger size = 0;
    const PDFSignature::ByteRanges& byteRanges = signature.getByteRanges();
    for (const PDFSignature::ByteRange& byteRange : byteRanges)
    {
        size += byteRange.size;
    }

    // Sanity checks
    if (size > sourceData.size())
    {
        result.addSignatureDataCoveredBySignatureMissingError();
        return nullptr;
    }

    PDFClosedIntervalSet bytesCoveredBySignature;

    outputBuffer.reserve(size);
    for (const PDFSignature::ByteRange& byteRange : byteRanges)
    {
        PDFInteger startOffset = byteRange.offset; // Offset to the first data byte
        PDFInteger endOffset = byteRange.offset + byteRange.size; // Offset to the byte following last data byte

        if (startOffset == endOffset)
        {
            // This means byte range is zero
            continue;
        }

        if (startOffset > endOffset || startOffset < 0 || endOffset < 0 || startOffset >= m_sourceData.size() || endOffset > m_sourceData.size())
        {
            result.addSignatureDataCoveredBySignatureMissingError();
            return nullptr;
        }

        const int length = endOffset - startOffset;
        outputBuffer.append(sourceData.constData() + startOffset, length);
        bytesCoveredBySignature.addInterval(startOffset, endOffset - 1);
    }

    // Jakub Melka: We must find byte string, which corresponds to signature.
    // We find only first occurence, because second one should not exist - because
    // it will mean that signature must be covered by itself.
    QByteArray hexContents = contents.toHex();
    int index = sourceData.indexOf(hexContents);
    if (index == -1)
    {
        index = sourceData.indexOf(hexContents.toUpper());
    }
    if (index != -1)
    {
        int firstByteIndex = index;
        int lastByteIndex = index + hexContents.size() - 1;

        if (firstByteIndex > 0 && sourceData[firstByteIndex - 1] == '<')
        {
            --firstByteIndex;
        }
        if (lastByteIndex + 1 < sourceData.size() && sourceData[lastByteIndex + 1] == '>')
        {
            ++lastByteIndex;
        }
        bytesCoveredBySignature.addInterval(firstByteIndex, lastByteIndex);
    }

    // We add a warning, that this signature doesn't cover whole source byte range
    if (!bytesCoveredBySignature.isCovered(0, sourceData.size() - 1))
    {
        const PDFInteger notCoveredBytes = sourceData.size() - int(bytesCoveredBySignature.getTotalLength());
        result.addSignatureNotCoveredBytesWarning(notCoveredBytes);
    }

    return BIO_new_mem_buf(outputBuffer.data(), outputBuffer.length());
}

void PDFPublicKeySignatureHandler::verifySignature(PDFSignatureVerificationResult& result) const
{
    PDFOpenSSLGlobalLock lock;

    OpenSSL_add_all_algorithms();

    const PDFSignature& signature = m_signatureField->getSignature();
    const QByteArray& content = signature.getContents();

    // Jakub Melka: we will try to get pkcs7 from signature, then
    // verify signer certificates.
    const unsigned char* data = reinterpret_cast<const unsigned char*>(content.data());
    if (PKCS7* pkcs7 = d2i_PKCS7(nullptr, &data, content.size()))
    {
        QByteArray buffer;
        if (BIO* inputBuffer = getSignedDataBuffer(result, buffer))
        {
            if (BIO* dataBio = PKCS7_dataInit(pkcs7, inputBuffer))
            {
                // Now, we must read from bio to calculate digests (digest is returned)
                std::array<char, 16384> buffer = { };
                int bytesRead = 0;
                do
                {
                    bytesRead = BIO_read(dataBio, buffer.data(), int(buffer.size()));
                } while (bytesRead > 0);

                STACK_OF(PKCS7_SIGNER_INFO)* signerInfo = PKCS7_get_signer_info(pkcs7);
                const int signerInfoCount = sk_PKCS7_SIGNER_INFO_num(signerInfo);
                STACK_OF(X509)* certificates = getCertificates(pkcs7);
                if (signerInfo && signerInfoCount > 0 && certificates)
                {
                    for (int i = 0; i < signerInfoCount; ++i)
                    {
                        PKCS7_SIGNER_INFO* signerInfoValue = sk_PKCS7_SIGNER_INFO_value(signerInfo, i);
                        PKCS7_ISSUER_AND_SERIAL* issuerAndSerial = signerInfoValue->issuer_and_serial;
                        X509* signer = X509_find_by_issuer_and_serial(certificates, issuerAndSerial->issuer, issuerAndSerial->serial);

                        if (!signer)
                        {
                            result.addSignatureCertificateMissingError();
                            break;
                        }

                        const int verification = PKCS7_signatureVerify(dataBio, pkcs7, signerInfoValue, signer);
                        if (verification <= 0)
                        {
                            const int reason = ERR_GET_REASON(ERR_get_error());
                            switch (reason)
                            {
                                case PKCS7_R_DIGEST_FAILURE:
                                    result.addSignatureDigestFailureError();
                                    break;

                                default:
                                    result.addSignatureDataOtherError();
                                    break;
                            }
                        }
                    }
                }
                else
                {
                    result.addSignatureNoSignaturesFoundError();
                }

                // According to the documentation, we should not call PKCS7_dataFinal
                // at the end, when pkcs7 is populated.

                BIO_free(dataBio);
            }
            else
            {
                result.addInvalidSignatureError();
            }

            BIO_free(inputBuffer);
        }
        else
        {
            // There is no need for adding error, error is in this case added by getSignedDataBuffer function
        }

        PKCS7_free(pkcs7);
    }
    else
    {
        result.addInvalidSignatureError();
    }

    if (!result.hasSignatureError())
    {
        result.setFlag(PDFSignatureVerificationResult::Signature_OK, true);
    }
}

PDFSignatureVerificationResult PDFSignatureHandler_adbe_pkcs7_detached::verify() const
{
    PDFSignatureVerificationResult result;
    initializeResult(result);
    verifyCertificate(result);
    verifySignature(result);
    result.validate();
    return result;
}

PDFSignatureVerificationResult PDFSignatureHandler_adbe_pkcs7_rsa_sha1::verify() const
{
    PDFSignatureVerificationResult result;
    initializeResult(result);

    verifyRSACertificate(result);
    verifyRSASignature(result);

    result.validate();
    return result;
}

X509* PDFSignatureHandler_adbe_pkcs7_rsa_sha1::createCertificate(size_t index) const
{
    const PDFSignature& signature = m_signatureField->getSignature();
    const std::vector<QByteArray>* certificates = signature.getCertificates();
    if (certificates && index < certificates->size())
    {
        const QByteArray& certificateSize = (*certificates)[index];
        const unsigned char* data = reinterpret_cast<const unsigned char*>(certificateSize.data());
        return d2i_X509(nullptr, &data, certificateSize.size());
    }

    return nullptr;
}

bool PDFSignatureHandler_adbe_pkcs7_rsa_sha1::getMessageDigest(const QByteArray& message,
                                                               ASN1_OCTET_STRING* encryptedString,
                                                               RSA* rsa,
                                                               int& algorithmNID,
                                                               QByteArray& digest) const
{
    if (!getMessageDigestAlgorithm(encryptedString, rsa, algorithmNID))
    {
        return false;
    }

    if (const EVP_MD* md = EVP_get_digestbynid(algorithmNID))
    {
        unsigned int messageDigestSize = EVP_MD_size(md);
        digest.resize(messageDigestSize);

        EVP_MD_CTX* context = EVP_MD_CTX_new();
        Q_ASSERT(context);

        EVP_DigestInit(context, md);
        EVP_DigestUpdate(context, message.constData(), message.size());
        EVP_DigestFinal(context, reinterpret_cast<unsigned char*>(digest.data()), &messageDigestSize);

        EVP_MD_CTX_free(context);
        return true;
    }

    return false;
}

bool PDFSignatureHandler_adbe_pkcs7_rsa_sha1::getMessageDigestAlgorithm(ASN1_OCTET_STRING* encryptedString,
                                                                        RSA* rsa,
                                                                        int& algorithmNID) const
{
    algorithmNID = 0;

    int size = RSA_size(rsa);
    std::vector<unsigned char> decryptedBuffer(size, 0);
    const int signatureSize = RSA_public_decrypt(encryptedString->length, encryptedString->data, decryptedBuffer.data(), rsa, RSA_PKCS1_PADDING);

    if (signatureSize <= 0)
    {
        return false;
    }

    Q_ASSERT(signatureSize < decryptedBuffer.size());

    const unsigned char* decryptedBufferPtr = decryptedBuffer.data();
    if (X509_SIG* x509_sig = d2i_X509_SIG(nullptr, &decryptedBufferPtr, signatureSize))
    {
        const X509_ALGOR* algorithm = nullptr;
        const ASN1_OBJECT* algorithmDescriptor = nullptr;

        X509_SIG_get0(x509_sig, &algorithm, nullptr);
        X509_ALGOR_get0(&algorithmDescriptor, nullptr, nullptr, algorithm);
        algorithmNID = OBJ_obj2nid(algorithmDescriptor);

        X509_SIG_free(x509_sig);
        return true;
    }

    return false;
}

void PDFSignatureHandler_adbe_pkcs7_rsa_sha1::verifyRSACertificate(PDFSignatureVerificationResult& result) const
{
    if (X509* certificate = createCertificate(0))
    {
        STACK_OF(X509)* certificates = sk_X509_new_null();
        sk_X509_push(certificates, certificate);

        for (size_t i = 1;; ++i)
        {
            if (X509* currentCertificate = createCertificate(i))
            {
                sk_X509_push(certificates, currentCertificate);
                X509_free(currentCertificate);
            }
            else
            {
                break;
            }
        }

        X509_STORE* store = X509_STORE_new();
        X509_STORE_CTX* context = X509_STORE_CTX_new();

        // Above functions can fail only if not enough memory. But in this
        // case, this library will crash anyway.
        Q_ASSERT(store);
        Q_ASSERT(context);

        addTrustedCertificates(store);

        X509* signer = certificate;
        if (!X509_STORE_CTX_init(context, store, signer, certificates))
        {
            result.addCertificateGenericError();
        }

        if (!X509_STORE_CTX_set_purpose(context, X509_PURPOSE_SMIME_SIGN))
        {
            result.addCertificateGenericError();
        }

        if (!result.hasCertificateError())
        {
            unsigned long flags = X509_V_FLAG_TRUSTED_FIRST;
            if (m_parameters.ignoreExpirationDate)
            {
                flags |= X509_V_FLAG_NO_CHECK_TIME;
            }
            X509_STORE_CTX_set_flags(context, flags);

            int verificationResult = X509_verify_cert(context);
            if (verificationResult <= 0)
            {
                int error = X509_STORE_CTX_get_error(context);
                switch (error)
                {
                    case X509_V_OK:
                        // Strange, this should not occur... when X509_verify_cert fails
                        break;

                    case X509_V_ERR_CERT_HAS_EXPIRED:
                        result.addCertificateExpiredError();
                        break;

                    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
                        result.addCertificateSelfSignedError();
                        break;

                    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
                        result.addCertificateSelfSignedInChainError();
                        break;

                    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
                    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
                        result.addCertificateTrustedNotFoundError();
                        break;

                    case X509_V_ERR_CERT_REVOKED:
                        result.addCertificateRevokedError();
                        break;

                    default:
                        result.addCertificateOtherError(error);
                        break;
                }

                // We will add certificate info for all certificates
                const int count = sk_X509_num(certificates);
                for (int i = 0; i < count; ++i)
                {
                    result.addCertificateInfo(getCertificateInfo(sk_X509_value(certificates, i)));
                }
            }
            else
            {
                STACK_OF(X509)* validChain = X509_STORE_CTX_get0_chain(context);
                const int count = sk_X509_num(validChain);
                for (int i = 0; i < count; ++i)
                {
                    result.addCertificateInfo(getCertificateInfo(sk_X509_value(validChain, i)));
                }
            }

            X509_STORE_CTX_cleanup(context);
        }

        X509_STORE_CTX_free(context);
        X509_STORE_free(store);

        sk_X509_free(certificates);
        X509_free(certificate);
    }
    else
    {
        result.addInvalidCertificateError();
    }

    if (!result.hasCertificateError())
    {
        result.setFlag(PDFSignatureVerificationResult::Certificate_OK, true);
    }
}

void PDFSignatureHandler_adbe_pkcs7_rsa_sha1::verifyRSASignature(PDFSignatureVerificationResult& result) const
{
    // Jakub Melka: we will use first certificate to validate signature
    X509* certificate = createCertificate(0);
    if (!certificate)
    {
        result.addSignatureCertificateMissingError();
        return;
    }

    EVP_PKEY* evpKey = X509_get0_pubkey(certificate);
    if (!evpKey)
    {
        X509_free(certificate);
        result.addSignatureCertificateMissingError();
        return;
    }

    RSA* rsa = EVP_PKEY_get0_RSA(evpKey);
    if (!rsa)
    {
        X509_free(certificate);
        result.addSignatureCertificateMissingError();
        return;
    }

    QByteArray outputBuffer;
    if (BIO* bio = this->getSignedDataBuffer(result, outputBuffer))
    {
        const PDFSignature& signature = m_signatureField->getSignature();
        const QByteArray& signKey = signature.getContents();

        const unsigned char* encryptedSign = reinterpret_cast<const unsigned char*>(signKey.constData());
        const unsigned int encryptedSignLength = signKey.length();
        if (ASN1_OCTET_STRING* encryptedString = d2i_ASN1_OCTET_STRING(nullptr, &encryptedSign, encryptedSignLength))
        {
            int algorithmNID = 0;
            QByteArray digestBuffer;
            if (!getMessageDigest(outputBuffer, encryptedString, rsa, algorithmNID, digestBuffer))
            {
                BIO_free(bio);
                X509_free(certificate);
                ASN1_OCTET_STRING_free(encryptedString);
                result.addSignatureDataOtherError();
                return;
            }

            const unsigned char* digest = reinterpret_cast<const unsigned char*>(digestBuffer.constData());
            const unsigned int digestLength = digestBuffer.length();

            const int verifyValue = RSA_verify(algorithmNID, digest, digestLength, encryptedString->data, encryptedString->length, rsa);
            ASN1_OCTET_STRING_free(encryptedString);

            if (verifyValue == 0)
            {
                // We have failed, probably due to invalid signature
                const unsigned long errorCode = ERR_GET_REASON(ERR_get_error());

                switch (errorCode)
                {
                    case RSA_R_DIGEST_DOES_NOT_MATCH:
                        result.addSignatureDigestFailureError();
                        break;

                    default:
                        result.addSignatureDataOtherError();
                        break;
                }
            }
        }
        else
        {
            result.addSignatureDataOtherError();
        }

        BIO_free(bio);
    }

    X509_free(certificate);

    if (!result.hasSignatureError())
    {
        result.setFlag(PDFSignatureVerificationResult::Signature_OK, true);
    }
}

PDFSignatureVerificationResult PDFSignatureHandler_adbe_pkcs7_sha1::verify() const
{
    PDFSignatureVerificationResult result;
    initializeResult(result);
    verifyCertificate(result);
    verifySignature(result);
    result.validate();
    return result;
}

BIO* PDFSignatureHandler_adbe_pkcs7_sha1::getSignedDataBuffer(PDFSignatureVerificationResult& result, QByteArray& outputBuffer) const
{
    QByteArray temporaryBuffer;
    if (BIO* bio = PDFPublicKeySignatureHandler::getSignedDataBuffer(result, temporaryBuffer))
    {
        // Calculate SHA1
        outputBuffer.resize(SHA_DIGEST_LENGTH);
        SHA1(reinterpret_cast<const unsigned char*>(temporaryBuffer.data()), temporaryBuffer.length(), reinterpret_cast<unsigned char*>(outputBuffer.data()));
        BIO_free(bio);

        return BIO_new_mem_buf(outputBuffer.data(), outputBuffer.length());
    }

    return nullptr;
}

PDFCertificateInfo PDFPublicKeySignatureHandler::getCertificateInfo(X509* certificate)
{
    PDFCertificateInfo info;

    if (X509_NAME* subjectName = X509_get_subject_name(certificate))
    {
        // List of these properties are in RFC 5280, section 4.1.2.4, these attributes
        // are standard and all implementations must be prepared to process them.
        QString countryName = getStringFromX509Name(subjectName, NID_countryName);
        QString organizationName = getStringFromX509Name(subjectName, NID_organizationName);
        QString organizationalUnitName = getStringFromX509Name(subjectName, NID_organizationalUnitName);
        QString distinguishedName = getStringFromX509Name(subjectName, NID_distinguishedName);
        QString stateOrProvinceName = getStringFromX509Name(subjectName, NID_stateOrProvinceName);
        QString commonName = getStringFromX509Name(subjectName, NID_commonName);
        QString serialNumber = getStringFromX509Name(subjectName, NID_serialNumber);

        // These attributes are defined also in section 4.1.2.4, they are not mandatory,
        // but application should be able to process them.
        QString localityName = getStringFromX509Name(subjectName, NID_localityName);
        QString title = getStringFromX509Name(subjectName, NID_title);
        QString surname = getStringFromX509Name(subjectName, NID_surname);
        QString givenName = getStringFromX509Name(subjectName, NID_givenName);
        QString initials = getStringFromX509Name(subjectName, NID_initials);
        QString pseudonym = getStringFromX509Name(subjectName, NID_pseudonym);
        QString generationQualifier = getStringFromX509Name(subjectName, NID_generationQualifier);

        // This entry is not defined in section 4.1.2.4, but is commonly used
        QString email = getStringFromX509Name(subjectName, NID_pkcs9_emailAddress);

        info.setName(PDFCertificateInfo::CountryName, qMove(countryName));
        info.setName(PDFCertificateInfo::OrganizationName, qMove(organizationName));
        info.setName(PDFCertificateInfo::OrganizationalUnitName, qMove(organizationalUnitName));
        info.setName(PDFCertificateInfo::DistinguishedName, qMove(distinguishedName));
        info.setName(PDFCertificateInfo::StateOrProvinceName, qMove(stateOrProvinceName));
        info.setName(PDFCertificateInfo::CommonName, qMove(commonName));
        info.setName(PDFCertificateInfo::SerialNumber, qMove(serialNumber));

        info.setName(PDFCertificateInfo::LocalityName, qMove(localityName));
        info.setName(PDFCertificateInfo::Title, qMove(title));
        info.setName(PDFCertificateInfo::Surname, qMove(surname));
        info.setName(PDFCertificateInfo::GivenName, qMove(givenName));
        info.setName(PDFCertificateInfo::Initials, qMove(initials));
        info.setName(PDFCertificateInfo::Pseudonym, qMove(pseudonym));
        info.setName(PDFCertificateInfo::GenerationalQualifier, qMove(generationQualifier));

        info.setName(PDFCertificateInfo::Email, qMove(email));

        const long version = X509_get_version(certificate);
        info.setVersion(version);

        const ASN1_TIME* notBeforeTime = X509_get0_notBefore(certificate);
        const ASN1_TIME* notAfterTime = X509_get0_notAfter(certificate);

        info.setNotValidBefore(getDateTimeFromASN(notBeforeTime));
        info.setNotValidAfter(getDateTimeFromASN(notAfterTime));

        X509_PUBKEY* publicKey = X509_get_X509_PUBKEY(certificate);
        EVP_PKEY* evpKey = X509_PUBKEY_get(publicKey);
        const int keyType = EVP_PKEY_type(EVP_PKEY_base_id(evpKey));

        PDFCertificateInfo::PublicKey key = PDFCertificateInfo::KeyUnknown;
        switch (keyType)
        {
            case EVP_PKEY_RSA:
                key = PDFCertificateInfo::KeyRSA;
                break;

            case EVP_PKEY_DSA:
                key = PDFCertificateInfo::KeyDSA;
                break;

            case EVP_PKEY_DH:
                key = PDFCertificateInfo::KeyDH;
                break;

            case EVP_PKEY_EC:
                key = PDFCertificateInfo::KeyEC;
                break;

            default:
                break;
        }
        info.setPublicKey(key);

        const int bits = EVP_PKEY_bits(evpKey);
        info.setKeySize(bits);

        const uint32_t keyUsage = X509_get_key_usage(certificate);
        if (keyUsage != UINT32_MAX)
        {
            static_assert(PDFCertificateInfo::KeyUsageDigitalSignature    == KU_DIGITAL_SIGNATURE, "Fix this code!");
            static_assert(PDFCertificateInfo::KeyUsageNonRepudiation      == KU_NON_REPUDIATION, "Fix this code!");
            static_assert(PDFCertificateInfo::KeyUsageKeyEncipherment     == KU_KEY_ENCIPHERMENT, "Fix this code!");
            static_assert(PDFCertificateInfo::KeyUsageDataEncipherment    == KU_DATA_ENCIPHERMENT, "Fix this code!");
            static_assert(PDFCertificateInfo::KeyUsageAgreement           == KU_KEY_AGREEMENT, "Fix this code!");
            static_assert(PDFCertificateInfo::KeyUsageCertSign            == KU_KEY_CERT_SIGN, "Fix this code!");
            static_assert(PDFCertificateInfo::KeyUsageCrlSign             == KU_CRL_SIGN, "Fix this code!");
            static_assert(PDFCertificateInfo::KeyUsageEncipherOnly        == KU_ENCIPHER_ONLY, "Fix this code!");
            static_assert(PDFCertificateInfo::KeyUsageDecipherOnly        == KU_DECIPHER_ONLY, "Fix this code!");

            info.setKeyUsage(static_cast<PDFCertificateInfo::KeyUsageFlags>(keyUsage));
        }

        unsigned char* buffer = nullptr;
        int length = i2d_X509(certificate, &buffer);
        if (length >= 0)
        {
            Q_ASSERT(buffer);
            info.setCertificateData(QByteArray(reinterpret_cast<const char*>(buffer), length));
            OPENSSL_free(buffer);
        }
    }

    return info;
}

void PDFCertificateInfo::serialize(QDataStream& stream) const
{
    stream << persist_version;
    stream << m_version;
    stream << m_keySize;
    stream << m_publicKey;
    stream << m_nameEntries;
    stream << m_notValidBefore;
    stream << m_notValidAfter;
    stream << m_keyUsage;
    stream << m_certificateData;
}

void PDFCertificateInfo::deserialize(QDataStream& stream)
{
    int persist_version = 0;
    stream >> persist_version;
    stream >> m_version;
    stream >> m_keySize;
    stream >> m_publicKey;
    stream >> m_nameEntries;
    stream >> m_notValidBefore;
    stream >> m_notValidAfter;
    stream >> m_keyUsage;
    stream >> m_certificateData;
}

QDateTime PDFCertificateInfo::getNotValidBefore() const
{
    return m_notValidBefore;
}

void PDFCertificateInfo::setNotValidBefore(const QDateTime& notValidBefore)
{
    m_notValidBefore = notValidBefore;
}

QDateTime PDFCertificateInfo::getNotValidAfter() const
{
    return m_notValidAfter;
}

void PDFCertificateInfo::setNotValidAfter(const QDateTime& notValidAfter)
{
    m_notValidAfter = notValidAfter;
}

int32_t PDFCertificateInfo::getVersion() const
{
    return m_version;
}

void PDFCertificateInfo::setVersion(int32_t version)
{
    m_version = version;
}

PDFCertificateInfo::PublicKey PDFCertificateInfo::getPublicKey() const
{
    return m_publicKey;
}

void PDFCertificateInfo::setPublicKey(const PublicKey& publicKey)
{
    m_publicKey = publicKey;
}

int PDFCertificateInfo::getKeySize() const
{
    return m_keySize;
}

void PDFCertificateInfo::setKeySize(int keySize)
{
    m_keySize = keySize;
}

PDFCertificateInfo::KeyUsageFlags PDFCertificateInfo::getKeyUsage() const
{
    return m_keyUsage;
}

void PDFCertificateInfo::setKeyUsage(KeyUsageFlags keyUsage)
{
    m_keyUsage = keyUsage;
}

std::optional<PDFCertificateInfo> PDFCertificateInfo::getCertificateInfo(const QByteArray& certificateData)
{
    std::optional<PDFCertificateInfo> result;

    PDFOpenSSLGlobalLock lock;
    const unsigned char* data = reinterpret_cast<const unsigned char*>(certificateData.constData());
    if (X509* certificate = d2i_X509(nullptr, &data, certificateData.length()))
    {
        result = PDFPublicKeySignatureHandler::getCertificateInfo(certificate);
        X509_free(certificate);
    }

    return result;
}

QByteArray PDFCertificateInfo::getCertificateData() const
{
    return m_certificateData;
}

void PDFCertificateInfo::setCertificateData(const QByteArray& certificateData)
{
    m_certificateData = certificateData;
}

void PDFCertificateStore::CertificateEntry::serialize(QDataStream& stream) const
{
    stream << persist_version;
    stream << type;
    stream << info;
}

void PDFCertificateStore::CertificateEntry::deserialize(QDataStream& stream)
{
    int persist_version = 0;
    stream >> persist_version;
    stream >> type;
    stream >> info;
}

QString PDFPublicKeySignatureHandler::getStringFromX509Name(X509_NAME* name, int nid)
{
    QString result;

    const int stringLocation = X509_NAME_get_index_by_NID(name, nid, -1);
    X509_NAME_ENTRY* entry = X509_NAME_get_entry(name, stringLocation);
    if (ASN1_STRING* string = X509_NAME_ENTRY_get_data(entry))
    {
        // Jakub Melka: we must convert entry to UTF8 encoding using function ASN1_STRING_to_UTF8
        unsigned char* utf8Buffer = nullptr;
        int errorCodeOrLength = ASN1_STRING_to_UTF8(&utf8Buffer, string);
        if (errorCodeOrLength > 0)
        {
            result = QString::fromUtf8(reinterpret_cast<const char*>(utf8Buffer), errorCodeOrLength);
        }
        OPENSSL_free(utf8Buffer);
    }

    return result;
}

QDateTime PDFPublicKeySignatureHandler::getDateTimeFromASN(const ASN1_TIME* time)
{
    QDateTime result;

    if (time)
    {
        tm internalTime = { };
        if (ASN1_TIME_to_tm(time, &internalTime) > 0)
        {
            time_t localTime = _mkgmtime(&internalTime);
            result = QDateTime::fromSecsSinceEpoch(localTime, Qt::UTC);
        }
    }

    return result;
}

void PDFCertificateStore::serialize(QDataStream& stream) const
{
    stream << persist_version;
    stream << m_certificates;
}

void pdf::PDFCertificateStore::deserialize(QDataStream& stream)
{
    int persist_version = 0;
    stream >> persist_version;
    stream >> m_certificates;
}

bool PDFCertificateStore::add(EntryType type, const QByteArray& certificate)
{
    if (auto certificateInfo = PDFCertificateInfo::getCertificateInfo(certificate))
    {
        return add(type, qMove(*certificateInfo));
    }

    return false;
}

bool PDFCertificateStore::add(EntryType type, PDFCertificateInfo info)
{
    auto it = std::find_if(m_certificates.cbegin(), m_certificates.cend(), [&info](const auto& entry) { return entry.info == info; });
    if (it == m_certificates.cend())
    {
        m_certificates.push_back({ type, qMove(info) });
    }

    return true;
}

bool pdf::PDFCertificateStore::contains(const pdf::PDFCertificateInfo& info)
{
    return std::find_if(m_certificates.cbegin(), m_certificates.cend(), [&info](const auto& entry) { return entry.info == info; }) != m_certificates.cend();
}

}   // namespace pdf

#ifdef Q_OS_WIN
#include <Windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#endif

void pdf::PDFPublicKeySignatureHandler::addTrustedCertificates(X509_STORE* store) const
{
    if (m_parameters.store)
    {
        const PDFCertificateStore::CertificateEntries& certificates = m_parameters.store->getCertificates();
        for (const auto& entry : certificates)
        {
            QByteArray certificateData = entry.info.getCertificateData();
            const unsigned char* pointer = reinterpret_cast<const unsigned char*>(certificateData.constData());
            X509* certificate = d2i_X509(nullptr, &pointer, certificateData.length());
            if (certificate)
            {
                X509_STORE_add_cert(store, certificate);
                X509_free(certificate);
            }
        }
    }

#ifdef Q_OS_WIN
    if (m_parameters.useSystemCertificateStore)
    {
        HCERTSTORE certStore = CertOpenSystemStore(NULL, L"ROOT");
        PCCERT_CONTEXT context = nullptr;
        if (certStore)
        {
            while (context = CertEnumCertificatesInStore(certStore, context))
            {
                const unsigned char* pointer = context->pbCertEncoded;
                X509* certificate = d2i_X509(nullptr, &pointer, context->cbCertEncoded);
                if (certificate)
                {
                    X509_STORE_add_cert(store, certificate);
                    X509_free(certificate);
                }
            }

            CertCloseStore(certStore, CERT_CLOSE_STORE_FORCE_FLAG);
        }
    }
#endif
}
